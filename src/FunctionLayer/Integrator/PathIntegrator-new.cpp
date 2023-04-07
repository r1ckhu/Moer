/**
 * @file PathIntegrator-new.cpp
 * @author Chenxi Zhou
 * @brief Path Integrator with new implementations
 * @version 0.1
 * @date 2022-09-21
 * 
 * @copyright NJUMeta (c) 2022 
 * www.njumeta.com
 */

#include "PathIntegrator-new.h"
#include "FastMath.h"
#include "FunctionLayer/Material/NullMaterial.h"

PathIntegratorNew::PathIntegratorNew(std::shared_ptr<Camera> _camera,
                                     std::unique_ptr<Film> _film,
                                     std::unique_ptr<TileGenerator> _tileGenerator,
                                     std::shared_ptr<Sampler> _sampler,
                                     int _spp,
                                     int _renderThreadNum) : 
    AbstractPathIntegrator(_camera, 
                           std::move(_film), 
                           std::move(_tileGenerator), 
                           _sampler, _spp, 
                           _renderThreadNum)
{

}

Spectrum PathIntegratorNew::Li(const Ray &initialRay, 
                               std::shared_ptr<Scene> scene) 
{
    const double eps = 1e-4;
    Spectrum L{.0};
    Spectrum throughput{1.0};
    Ray ray = initialRay;
    int nBounces = 0;
    auto itsOpt = scene->intersect(ray);

    while (true) {

        //* All rays generated by bsdf/phase sampling which might hold radiance
        //* will be counted at the end of the loop
        //* except the ray generated by camera which is considered here.
        //* The radiance of environment map will be counted while no intersection is found but not here.
        if (nBounces == 0) {
            if (itsOpt.has_value()) {
                PathIntegratorLocalRecord evalLightRecord = evalEmittance(scene, itsOpt, ray);
                L += throughput * evalLightRecord.f;
            }    
        }

        //* No intersection. Add possible radiance from environment map.
        if (!itsOpt.has_value()){
            auto envMapRecord=evalEmittance(scene,itsOpt,ray);
            L += throughput * envMapRecord.f;
            break;
        }
        
        auto its = itsOpt.value();

        nBounces++;

        // * Ignore null materials using isNull() flag.
        if(its.material->getBxDF(its)->isNull()){
            nBounces--;
            // * hint: ray should be immersed in medium. However, PathIntegrator will ignore any medium.
            ray = Ray{its.position + ray.direction * eps, ray.direction};
            itsOpt=scene->intersect(ray);
            continue;
        }

        // Russian roulette.
        double pSurvive = russianRoulette(throughput, nBounces);
        if (randFloat() > pSurvive)
            break;
        throughput /= pSurvive;

        //* ----- Direct Illumination -----
        for (int i = 0; i < nDirectLightSamples; ++i) {
            PathIntegratorLocalRecord sampleLightRecord = sampleDirectLighting(scene, its, ray);
            PathIntegratorLocalRecord evalScatterRecord = evalScatter(its, ray, sampleLightRecord.wi);

            if (!sampleLightRecord.f.isBlack()) {
                //* Multiple importance sampling
                double misw = MISWeight(sampleLightRecord.pdf, evalScatterRecord.pdf);
                if (sampleLightRecord.isDelta) {
                    // * MIS will not be applied with delta distribution of light source.
                    misw = 1.0;
                }
                L += throughput * sampleLightRecord.f * evalScatterRecord.f 
                     / sampleLightRecord.pdf * misw
                     / nDirectLightSamples;
            }
        }

        //*----- BSDF Sampling -----
        PathIntegratorLocalRecord sampleScatterRecord = sampleScatter(its, ray);
        if (!sampleScatterRecord.f.isBlack()) {
            throughput *= sampleScatterRecord.f / sampleScatterRecord.pdf;
        } else {
            break;
        }

        //* Another part of MIS, Test whether the bsdf sampling ray hit the emitter
        //* Another part of MIS, Test whether the bsdf sampling ray hit the emitter
        ray = Ray{its.position + sampleScatterRecord.wi * eps, sampleScatterRecord.wi};
        itsOpt = scene->intersect(ray);

        auto evalLightRecord = evalEmittance(scene, itsOpt, ray);
        if (itsOpt.has_value() && !evalLightRecord.f.isBlack()) {
            //* The continuous ray hit the emitter
            //* Multiple importance samplingy 

            double misw = MISWeight(sampleScatterRecord.pdf, evalLightRecord.pdf);
            if (sampleScatterRecord.isDelta) {
                //* MIS will not be applied with delta distribution of BSDF.
                misw = 1.0;
            }

            L += throughput * evalLightRecord.f * misw;
        }
        
    }

    return L;

}

/// @brief Eval surface or infinite light source (on itsOpt) radiance and ignore medium transmittance.
/// @param scene Scene description. Used to query scene lighting condition.
/// @param itsOpt Current intersection point which could be a light source. If there's no intersection, eval the radiance of environment light.
/// @param ray Current ray which connects last intersection point and itsOpt.
/// @return Current ray direction, obtained light radiance and solid angle dependent pdf. Note that there is no corresponding sampling process for pdf and the pdf value should NOT be applied to calculate the final radiance contribution.
/// @brief Eval surface or infinite light source (on itsOpt) radiance and ignore medium transmittance.
/// @param scene Scene description. Used to query scene lighting condition.
/// @param itsOpt Current intersection point which could be a light source. If there's no intersection, eval the radiance of environment light.
/// @param ray Current ray which connects last intersection point and itsOpt.
/// @return Current ray direction, obtained light radiance and solid angle dependent pdf. Note that there is no corresponding sampling process for pdf and the pdf value should NOT be applied to calculate the final radiance contribution.
PathIntegratorLocalRecord PathIntegratorNew::evalEmittance(std::shared_ptr<Scene> scene, 
                                                           std::optional<Intersection> itsOpt, 
                                                           const Ray &ray)
{
    Vec3d wo = -ray.direction;
    Spectrum LEmission(0.0);
    double pdfDirect = 1.0;
    if (!itsOpt.has_value())
    {
        auto record = evalEnvLights(scene, ray);
        LEmission = record.f;
        pdfDirect = record.pdf;
    }
    else if (itsOpt.value().object && itsOpt.value().object->getLight())
    {
        auto its = itsOpt.value();
        Normal3d n = its.geometryNormal;
        auto light = itsOpt.value().object->getLight();
        auto record = light->eval(ray, its, ray.direction);
        LEmission = record.s;
        Intersection tmpIts;
        tmpIts.position = ray.origin;
        pdfDirect = record.pdfDirect * chooseOneLightPdf(scene, light);
    }
    // Path integrator will ignore medium transmittance. And (of course) there will be no occlusion.
    // Spectrum transmittance(1.0);
    return {ray.direction, LEmission, pdfDirect, false}; 
    // Path integrator will ignore medium transmittance. And (of course) there will be no occlusion.
    // Spectrum transmittance(1.0);
    return {ray.direction, LEmission, pdfDirect, false}; 

}

/// @brief Sample on the distribution of direct lighting and ignore medium transmittance.
/// @param scene Scene description. Multiple shadow ray intersect operations will be performed.
/// @param its Current intersection point which returned pdf dependent on.
/// @param ray Current ray. Should only be applied for time records.
/// @return Sampled direction on the distribution of direct lighting and corresponding solid angle dependent pdf. An extra flag indicites that whether it sampled on a delta distribution.
PathIntegratorLocalRecord PathIntegratorNew::sampleDirectLighting(std::shared_ptr<Scene> scene, 
                                                                  const Intersection &its, 
                                                                  const Ray &ray)
{
    auto [light, pdfChooseLight] = chooseOneLight(scene, sampler->sample1D());
    auto record = light->sampleDirect(its, sampler->sample2D(), ray.timeMin);
    double pdfDirect = record.pdfDirect * pdfChooseLight; // pdfScatter with respect to solid angle
    Vec3d dirScatter = record.wi;
    Spectrum Li = record.s;
    Point3d posL = record.dst;
    Point3d posS = its.position;
    Spectrum transmittance(1.0); // todo: transmittance eval
    Ray visibilityTestingRay(posL - dirScatter * 1e-4, -dirScatter, ray.timeMin, ray.timeMax);
    auto visibilityTestingIts = scene->intersect(visibilityTestingRay);
    if (!visibilityTestingIts.has_value() || visibilityTestingIts->object != its.object || (visibilityTestingIts->position - posS).length2() > 1e-6)
    {
        transmittance = 0.0;
    }
    if(!visibilityTestingIts.has_value() && light->lightType==ELightType::INFINITE){
        transmittance = 1.0;
    }
    return {dirScatter, Li * transmittance, pdfDirect, record.isDeltaPos};

}

/// @brief Eval the scattering function, i.e., bsdf * cos. The cosine term will not be counted for delta distributed bsdf (inside f).
/// @param its Current intersection point which is used to obtain local coordinate and bxdf.
/// @param ray Current ray which is used to calculate bsdf $f(\omega_i,\omega_o)$.
/// @param dirScatter (already sampled) scattering direction.
/// @return scattering direction, bsdf value and bsdf pdf. 
PathIntegratorLocalRecord PathIntegratorNew::evalScatter(const Intersection &its,
                                                        const Ray &ray,
                                                        const Vec3d &dirScatter)
{
    if (its.material != nullptr)
    {
        std::shared_ptr<BxDF> bxdf = its.material->getBxDF(its);
        Normal3d n = its.geometryNormal;
        double wiDotN = fm::abs(dot(n, dirScatter));
        Vec3d wi = its.toLocal(dirScatter);
        Vec3d wo = its.toLocal(-ray.direction);
        return {
            dirScatter,
            bxdf->f(wo, wi,false) * wiDotN,
            bxdf->pdf(wo, wi),
            false};
    }
    else
    {
        // Path integrator will igonre phase function distribution.
        // Path integrator will igonre phase function distribution.
        return {};
    }
}

/// @brief Sample a direction along with a pdf value in term of solid angle according to the distribution of bsdf.
/// @param its Current intersection point.
/// @param ray Current incident ray.
/// @return Sampled scattering direction, bsdf * cos, corresponding pdf and whether it is sampled on a delta distribution.
PathIntegratorLocalRecord PathIntegratorNew::sampleScatter(const Intersection &its,
                                                        const Ray &ray)
{
    if (its.material != nullptr)
    {
        Vec3d wo = its.toLocal(-ray.direction);
        std::shared_ptr<BxDF> bxdf = its.material->getBxDF(its);
        Vec3d n = its.geometryNormal;
        BxDFSampleResult bsdfSample = bxdf->sample(wo, sampler->sample2D(),false);
        double pdf = bsdfSample.pdf;
        Vec3d dirScatter = its.toWorld(bsdfSample.directionIn);
        double wiDotN = fm::abs(dot(dirScatter, n));
        return {dirScatter, bsdfSample.s * wiDotN, pdf, BxDF::MatchFlags(bsdfSample.bxdfSampleType,BXDF_SPECULAR)};
    }
    else
    {
        // todo: sample phase function
        return {};
    }
}

/// @brief Russian roulette method.
/// @param throughput Current thorughput, i.e., multiplicative (bsdf * cos) / pdf.
/// @param nBounce Current bounce depth.
/// @return Survive probility after Russian roulette.
double PathIntegratorNew::russianRoulette(
                                       const Spectrum &throughput,
                                       int nBounce)
{
    // double pSurvive = std::min(pRussianRoulette, throughput.sum());
    double pSurvive = pRussianRoulette;
    if (nBounce > nPathLengthLimit)
        pSurvive = 0.0;
    if (nBounce <= 20)
        pSurvive = 1.0;
    return pSurvive;
}

/// @brief (discretely) sample a light source.
/// @param scene Scene description which is used to query scene lights.
/// @param lightSample A random number within [0,1].
/// @return Pointer of the sampled light source and corresponding (discrete) probility.
std::pair<std::shared_ptr<Light>, double> 
PathIntegratorNew::chooseOneLight(std::shared_ptr<Scene> scene,
                                  double lightSample)
{
    // uniformly weighted
    std::shared_ptr<std::vector<std::shared_ptr<Light>>> lights = scene->getLights();
    int numLights = lights->size();
    int lightID = std::min(numLights - 1, (int)(lightSample * numLights));
    std::shared_ptr<Light> light = lights->operator[](lightID);
    return {light, 1.0 / numLights};
}

/// @brief Calculate the (discrete) probility that a specific light source is sampled.
/// @param scene Scene description which is used to query scene lights.
/// @param light The specific light source.
/// @return Corresponding (discrete) probility.
double PathIntegratorNew::chooseOneLightPdf(std::shared_ptr<Scene> scene,
                                            std::shared_ptr<Light> light)
{
    std::shared_ptr<std::vector<std::shared_ptr<Light>>> lights = scene->getLights();
    int numLights = lights->size();
    return 1.0 / numLights;
}

/// @brief Eval the effect of environment light source (infinite area light).
/// @param scene Scene description which is used to query scene lights.
/// @param ray Current ray.
/// @return light source direction, light radiance and pdf. Without sampling process, the pdf can NOT be used to calculate final radiance.
PathIntegratorLocalRecord PathIntegratorNew::evalEnvLights(std::shared_ptr<Scene> scene,
                                                           const Ray &ray)
{
    std::shared_ptr<std::vector<std::shared_ptr<Light>>> lights = scene->getLights();
    Spectrum L(0.0);
    double pdf = 0.0;
    for (auto light : *lights)
    {
        auto record = light->evalEnvironment(ray);
        L += record.s;
        pdf += record.pdfEmitDir;
    } 
    return {-ray.direction, L, pdf};
}
