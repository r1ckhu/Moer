/**
 * @file Entity.h
 * @author orbitchen
 * @brief Objects that can be intersected with ray.
 * @version 0.1
 * @date 2022-04-30
 *
 * @copyright Copyright (c) 2022
 *
 */
#pragma once

#include "CoreLayer/Geometry/Transform3d.h"
#include "CoreLayer/Ray/Ray.h"

#include <optional>
#include <memory>

struct Intersection;
class Light;

class Entity : public Transform3D
{
protected:
	std::shared_ptr<Light> lightPtr;

public:
	virtual std::optional<Intersection> intersect(const Ray &r) const = 0;
	// @brief Return ptr to light when primitive is a emitter. Otherwise, return nullptr.
	virtual std::shared_ptr<Light> getLight() const = 0;
	virtual void setLight(std::shared_ptr<Light> light) = 0;
	virtual double area() const = 0;
	virtual Intersection sample(const Point2d &positionSample) const = 0;
};