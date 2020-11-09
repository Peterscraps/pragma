/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2020 Florian Weischer
 */

#ifndef __OCCLUSION_CULLING_HANDLER_BRUTE_FORCE_HPP__
#define __OCCLUSION_CULLING_HANDLER_BRUTE_FORCE_HPP__

#include "pragma/clientdefinitions.h"
#include "pragma/rendering/occlusion_culling/occlusion_culling_handler.hpp"

namespace pragma
{
	class DLLCLIENT OcclusionCullingHandlerBruteForce
		: public OcclusionCullingHandler
	{
	public:
		OcclusionCullingHandlerBruteForce()=default;
		virtual void PerformCulling(
			CSceneComponent &scene,const rendering::RasterizationRenderer &renderer,const Vector3 &camPos,
			std::vector<pragma::OcclusionMeshInfo> &culledMeshesOut,bool cullByViewFrustum=true
		) override;
	private:
	};
};

#endif
