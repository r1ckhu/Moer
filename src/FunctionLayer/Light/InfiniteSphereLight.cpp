#include "InfiniteSphereLight.h"
#include "ResourceLayer/File/FileUtils.h"
Vec3d UvToDirection(const Point2d & uv,double  & sinTheta){
    double phi = ( uv.x-0.5f ) * 2* M_PI;
    double theta = uv.y * M_PI;
    sinTheta = std::sin(theta);
    return Vec3d (
            std::cos(phi)*sinTheta,
            -std::cos(theta),
            std::sin(phi)*sinTheta);
}

Point2d DirectionToUv(const Vec3d & direction){
    return Point2d(std::atan2(direction.z, direction.x)*0.5*INV_PI+0.5, std::acos(-direction.y)*INV_PI);
}

LightSampleResult InfiniteSphereLight::evalEnvironment(const Ray & ray) {


    //todo support environment
    LightSampleResult ans;

    TextureCoord2D textureCoord2D(DirectionToUv(ray.direction));
    ans.s = emission->eval(textureCoord2D);
    ans.src = ray.origin;
    ans.pdfDirect = 0.0;
    ans.pdfEmitPos = 0.0;
    ans.pdfEmitDir = 0.0;
    ans.isDeltaPos = false;
    ans.isDeltaDir = false;
    return ans;
}

LightSampleResult InfiniteSphereLight::eval(const Ray & ray, const Intersection & its, const Vec3d & d) {
    throw("Infinite sphere should not be intersected,so this Function should  never not be called.");
}

LightSampleResult
InfiniteSphereLight::sampleEmit(const Point2d & positionSample, const Point2d & directionSample, float time) {
    throw("Not implemented yet");
    return LightSampleResult();
}

LightSampleResult InfiniteSphereLight::sampleDirect(const Intersection & its, const Point2d & sample, float time) {
    LightSampleResult ans;
    //todo map from image
    double  pdf;
    Point2d  uv = distribution->SampleContinuous(sample,&pdf);

    TextureCoord2D  textureCoord2D(uv);
    ans.s = emission->eval(textureCoord2D);

    double  sinTheta;
    Vec3d  dir = UvToDirection(uv,sinTheta);

    ans.src =  its.position;
    double  _worldRadius = 100; //todo load radius from scene
    ans.dst = ans.src + 2 * _worldRadius * dir;
    ans.uv = uv;
    ans.pdfDirect = pdf/( 2 * M_PI * M_PI * sinTheta );
    ans.wi =  dir;

    ans.isDeltaPos = false;
    ans.isDeltaDir = false;

    return ans;
}

LightSampleResult InfiniteSphereLight::sampleDirect(const MediumSampleRecord & mRec, Point2d sample, double time) {
    throw("Not implemented yet");
    return LightSampleResult();
}

InfiniteSphereLight::InfiniteSphereLight(const Json & json) {
    if(json.contains("emission")){
        std::string workingDir = FileUtils::WorkingDir;
        std::string emissionTexturePath = FileUtils::WorkingDir+json.at("emission").get<std::string>();
        emission = std::make_shared<ImageTexture<Spectrum,RGB3>>(FileUtils::WorkingDir+json.at("emission").get<std::string>());
        int length = emission->getWidth()*emission->getHeight();
        double * weights = new double[length];
        std::fill(weights,weights+length,1);

        //todo sphere weights
        distribution = std::make_unique<Distribution2D>(weights,emission->getWidth(),emission->getHeight());
    }
    else{
        //todo report error
    }
}