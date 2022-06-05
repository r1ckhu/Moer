/**
 * @file PointLight.cpp
 * @author Zhimin Fan
 * @brief Point light.
 * @version 0.1
 * @date 2022-05-15
 *
 * @copyright Copyright (c) 2022
 *
 */

#include "PointLight.h"

#define DIRAC 1.0 // todo: delete it

LightSampleResult PointLight::evalEnvironment(const Ray &ray)
{
    LightSampleResult ans;
    ans.s = 0.0;
    ans.src = ray.origin;
    ans.dst = transform->getTranslate();
    ans.wi = normalize(ans.dst - ans.src);
    ans.pdfDirect = 0.0;
    ans.pdfEmitPos = 0.0;
    ans.pdfEmitDir = 0.0;
    ans.isDeltaPos = true;
    ans.isDeltaDir = false;
    return ans;
}

LightSampleResult PointLight::eval(const Ray& ray, const Intersection &its, const Vec3d &d)
{
    // This function should not be called.
    LightSampleResult ans;
    ans.s = 0.0;
    ans.src = ray.origin;
    ans.dst = its.position;
    ans.wi = d;
    ans.pdfDirect = 0.0;
    ans.pdfEmitPos = 0.0;
    ans.pdfEmitDir = 0.0;
    ans.isDeltaPos = true;
    ans.isDeltaDir = false;
    return ans;
}

LightSampleResult PointLight::sampleEmit(const Point2d &positionSample, const Point2d &directionSample, float time)
{
    Normal3d wi; // todo: convert directionSample to wi
    LightSampleResult ans;
    ans.s = intensity * DIRAC;
    ans.dst = transform->getTranslate();
    ans.wi = wi;
    ans.pdfEmitPos = 1.0 * DIRAC;
    ans.pdfEmitDir = 1.0 / 3.14159 / 4; // todo: replace with a constant pi
    ans.pdfDirect = 0.0;
    ans.isDeltaPos = true;
    ans.isDeltaDir = false;
    return ans;
}

LightSampleResult PointLight::sampleDirect(const Intersection &its, const Point2d &sample, float time)
{
    Normal3d wi = normalize(transform->getTranslate() - its.position);
    LightSampleResult ans;
    ans.src = its.position;
    ans.dst = transform->getTranslate();
    ans.s = intensity / (ans.dst - ans.src).length2() * DIRAC;
    ans.wi = wi;
    ans.pdfDirect = 1.0 * DIRAC;
    ans.isDeltaPos = true;
    ans.isDeltaDir = false;
    return ans;
}