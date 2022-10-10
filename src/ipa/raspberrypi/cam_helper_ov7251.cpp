/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021, Raspberry Pi Ltd
 *
 * cam_helper_ov7251.cpp - camera information for ov7251 sensor
 */

#include <assert.h>

#include "cam_helper.h"

using namespace RPiController;

class CamHelperOv7251 : public CamHelper
{
public:
	CamHelperOv7251();
	uint32_t gainCode(double gain) const override;
	double gain(uint32_t gainCode) const override;
	void getDelays(int &exposureDelay, int &gainDelay,
		       int &vblankDelay) const override;

private:
	/*
	 * Smallest difference between the frame length and integration time,
	 * in units of lines.
	 */
	static constexpr int frameIntegrationDiff = 4;
};

/*
 * OV7251 doesn't output metadata, so we have to use the "unicam parser" which
 * works by counting frames.
 */

CamHelperOv7251::CamHelperOv7251()
	: CamHelper({}, frameIntegrationDiff)
{
}

uint32_t CamHelperOv7251::gainCode(double gain) const
{
	return static_cast<uint32_t>(gain * 16.0);
}

double CamHelperOv7251::gain(uint32_t gainCode) const
{
	return static_cast<double>(gainCode) / 16.0;
}

void CamHelperOv7251::getDelays(int &exposureDelay, int &gainDelay,
				int &vblankDelay) const
{
	/* The driver appears to behave as follows: */
	exposureDelay = 2;
	gainDelay = 2;
	vblankDelay = 2;
}

static CamHelper *create()
{
	return new CamHelperOv7251();
}

static RegisterCamHelper reg("ov7251", &create);
