/* SPDX-License-Identifier: MIT */
/*
 * Copyright 2023 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "reg_helper.h"

#include "core_types.h"
#include "link_encoder.h"
#include "dcn31/dcn31_dio_link_encoder.h"
#include "dcn35_dio_link_encoder.h"
#include "dc_dmub_srv.h"
#define CTX \
	enc10->base.ctx
#define DC_LOGGER \
	enc10->base.ctx->logger

#define REG(reg)\
	(enc10->link_regs->reg)

#undef FN
#define FN(reg_name, field_name) \
	enc10->link_shift->field_name, enc10->link_mask->field_name
/*
 * @brief
 * Trigger Source Select
 * ASIC-dependent, actual values for register programming
 */
#define DCN35_DIG_FE_SOURCE_SELECT_INVALID 0x0
#define DCN35_DIG_FE_SOURCE_SELECT_DIGA 0x1
#define DCN35_DIG_FE_SOURCE_SELECT_DIGB 0x2
#define DCN35_DIG_FE_SOURCE_SELECT_DIGC 0x4
#define DCN35_DIG_FE_SOURCE_SELECT_DIGD 0x08
#define DCN35_DIG_FE_SOURCE_SELECT_DIGE 0x10


bool dcn35_is_dig_enabled(struct link_encoder *enc)
{
	uint32_t enabled;
	struct dcn10_link_encoder *enc10 = TO_DCN10_LINK_ENC(enc);

	REG_GET(DIG_BE_CLK_CNTL, DIG_BE_CLK_EN, &enabled);
	return (enabled == 1);
}

enum signal_type dcn35_get_dig_mode(
	struct link_encoder *enc)
{
	uint32_t value;
	struct dcn10_link_encoder *enc10 = TO_DCN10_LINK_ENC(enc);

	REG_GET(DIG_BE_CLK_CNTL, DIG_BE_MODE, &value);
	switch (value) {
	case 0:
		return SIGNAL_TYPE_DISPLAY_PORT;
	case 2:
		return SIGNAL_TYPE_DVI_SINGLE_LINK;
	case 3:
		return SIGNAL_TYPE_HDMI_TYPE_A;
	case 5:
		return SIGNAL_TYPE_DISPLAY_PORT_MST;
	default:
		return SIGNAL_TYPE_NONE;
	}
}

void dcn35_link_encoder_setup(
	struct link_encoder *enc,
	enum signal_type signal)
{
	struct dcn10_link_encoder *enc10 = TO_DCN10_LINK_ENC(enc);

	switch (signal) {
	case SIGNAL_TYPE_EDP:
	case SIGNAL_TYPE_DISPLAY_PORT:
		/* DP SST */
		REG_UPDATE(DIG_BE_CLK_CNTL, DIG_BE_MODE, 0);
		break;
	case SIGNAL_TYPE_DVI_SINGLE_LINK:
	case SIGNAL_TYPE_DVI_DUAL_LINK:
		/* TMDS-DVI */
		REG_UPDATE(DIG_BE_CLK_CNTL, DIG_BE_MODE, 2);
		break;
	case SIGNAL_TYPE_HDMI_TYPE_A:
		/* TMDS-HDMI */
		REG_UPDATE(DIG_BE_CLK_CNTL, DIG_BE_MODE, 3);
		break;
	case SIGNAL_TYPE_DISPLAY_PORT_MST:
		/* DP MST */
		REG_UPDATE(DIG_BE_CLK_CNTL, DIG_BE_MODE, 5);
		break;
	default:
		ASSERT_CRITICAL(false);
		/* invalid mode ! */
		break;
	}
	REG_UPDATE(DIG_BE_CLK_CNTL, DIG_BE_CLK_EN, 1);

}

void dcn35_link_encoder_init(struct link_encoder *enc)
{
	enc31_hw_init(enc);
	dcn35_link_encoder_set_fgcg(enc, enc->ctx->dc->debug.enable_fine_grain_clock_gating.bits.dio);
}

void dcn35_link_encoder_set_fgcg(struct link_encoder *enc, bool enable)
{
	struct dcn10_link_encoder *enc10 = TO_DCN10_LINK_ENC(enc);

	REG_UPDATE(DIO_CLK_CNTL, DIO_FGCG_REP_DIS, !enable);
}

static const struct link_encoder_funcs dcn35_link_enc_funcs = {
	.read_state = link_enc2_read_state,
	.validate_output_with_stream =
			dcn30_link_encoder_validate_output_with_stream,
	.hw_init = dcn35_link_encoder_init,
	.setup = dcn35_link_encoder_setup,
	.enable_tmds_output = dcn10_link_encoder_enable_tmds_output,
	.enable_dp_output = dcn35_link_encoder_enable_dp_output,
	.enable_dp_mst_output = dcn35_link_encoder_enable_dp_mst_output,
	.disable_output = dcn35_link_encoder_disable_output,
	.dp_set_lane_settings = dcn10_link_encoder_dp_set_lane_settings,
	.dp_set_phy_pattern = dcn10_link_encoder_dp_set_phy_pattern,
	.update_mst_stream_allocation_table =
		dcn10_link_encoder_update_mst_stream_allocation_table,
	.psr_program_dp_dphy_fast_training =
			dcn10_psr_program_dp_dphy_fast_training,
	.psr_program_secondary_packet = dcn10_psr_program_secondary_packet,
	.connect_dig_be_to_fe = dcn10_link_encoder_connect_dig_be_to_fe,
	.enable_hpd = dcn10_link_encoder_enable_hpd,
	.disable_hpd = dcn10_link_encoder_disable_hpd,
	.is_dig_enabled = dcn35_is_dig_enabled,
	.destroy = dcn10_link_encoder_destroy,
	.fec_set_enable = enc2_fec_set_enable,
	.fec_set_ready = enc2_fec_set_ready,
	.fec_is_active = enc2_fec_is_active,
	.get_dig_frontend = dcn10_get_dig_frontend,
	.get_dig_mode = dcn35_get_dig_mode,
	.is_in_alt_mode = dcn31_link_encoder_is_in_alt_mode,
	.get_max_link_cap = dcn31_link_encoder_get_max_link_cap,
	.set_dio_phy_mux = dcn31_link_encoder_set_dio_phy_mux,
	.enable_dpia_output = dcn35_link_encoder_enable_dpia_output,
	.disable_dpia_output = dcn35_link_encoder_disable_dpia_output,
};

void dcn35_link_encoder_construct(
	struct dcn20_link_encoder *enc20,
	const struct encoder_init_data *init_data,
	const struct encoder_feature_support *enc_features,
	const struct dcn10_link_enc_registers *link_regs,
	const struct dcn10_link_enc_aux_registers *aux_regs,
	const struct dcn10_link_enc_hpd_registers *hpd_regs,
	const struct dcn10_link_enc_shift *link_shift,
	const struct dcn10_link_enc_mask *link_mask)
{
	struct bp_connector_speed_cap_info bp_cap_info = {0};
	const struct dc_vbios_funcs *bp_funcs = init_data->ctx->dc_bios->funcs;
	enum bp_result result = BP_RESULT_OK;
	struct dcn10_link_encoder *enc10 = &enc20->enc10;

	enc10->base.funcs = &dcn35_link_enc_funcs;
	enc10->base.ctx = init_data->ctx;
	enc10->base.id = init_data->encoder;

	enc10->base.hpd_source = init_data->hpd_source;
	enc10->base.connector = init_data->connector;

	enc10->base.preferred_engine = ENGINE_ID_UNKNOWN;

	enc10->base.features = *enc_features;

	if (enc10->base.connector.id == CONNECTOR_ID_USBC)
		enc10->base.features.flags.bits.DP_IS_USB_C = 1;

	enc10->base.transmitter = init_data->transmitter;

	/* set the flag to indicate whether driver poll the I2C data pin
	 * while doing the DP sink detect
	 */

/*	if (dal_adapter_service_is_feature_supported(as,
 *		FEATURE_DP_SINK_DETECT_POLL_DATA_PIN))
 *		enc10->base.features.flags.bits.
 *			DP_SINK_DETECT_POLL_DATA_PIN = true;
 */

	enc10->base.output_signals =
		SIGNAL_TYPE_DVI_SINGLE_LINK |
		SIGNAL_TYPE_DVI_DUAL_LINK |
		SIGNAL_TYPE_LVDS |
		SIGNAL_TYPE_DISPLAY_PORT |
		SIGNAL_TYPE_DISPLAY_PORT_MST |
		SIGNAL_TYPE_EDP |
		SIGNAL_TYPE_HDMI_TYPE_A;

	enc10->link_regs = link_regs;
	enc10->aux_regs = aux_regs;
	enc10->hpd_regs = hpd_regs;
	enc10->link_shift = link_shift;
	enc10->link_mask = link_mask;

	switch (enc10->base.transmitter) {
	case TRANSMITTER_UNIPHY_A:
		enc10->base.preferred_engine = ENGINE_ID_DIGA;
	break;
	case TRANSMITTER_UNIPHY_B:
		enc10->base.preferred_engine = ENGINE_ID_DIGB;
	break;
	case TRANSMITTER_UNIPHY_C:
		enc10->base.preferred_engine = ENGINE_ID_DIGC;
	break;
	case TRANSMITTER_UNIPHY_D:
		enc10->base.preferred_engine = ENGINE_ID_DIGD;
	break;
	case TRANSMITTER_UNIPHY_E:
		enc10->base.preferred_engine = ENGINE_ID_DIGE;
	break;
	default:
		ASSERT_CRITICAL(false);
		enc10->base.preferred_engine = ENGINE_ID_UNKNOWN;
	}

	enc10->base.features.flags.bits.HDMI_6GB_EN = 1;

	if (bp_funcs->get_connector_speed_cap_info)
		result = bp_funcs->get_connector_speed_cap_info(enc10->base.ctx->dc_bios,
						enc10->base.connector, &bp_cap_info);

	/* Override features with DCE-specific values */
	if (result == BP_RESULT_OK) {
		enc10->base.features.flags.bits.IS_HBR2_CAPABLE =
				bp_cap_info.DP_HBR2_EN;
		enc10->base.features.flags.bits.IS_HBR3_CAPABLE =
				bp_cap_info.DP_HBR3_EN;
		enc10->base.features.flags.bits.HDMI_6GB_EN = bp_cap_info.HDMI_6GB_EN;
		enc10->base.features.flags.bits.IS_DP2_CAPABLE = 1;
		enc10->base.features.flags.bits.IS_UHBR10_CAPABLE = bp_cap_info.DP_UHBR10_EN;
		enc10->base.features.flags.bits.IS_UHBR13_5_CAPABLE = bp_cap_info.DP_UHBR13_5_EN;
		enc10->base.features.flags.bits.IS_UHBR20_CAPABLE = bp_cap_info.DP_UHBR20_EN;

	} else {
		DC_LOG_WARNING("%s: Failed to get encoder_cap_info from VBIOS with error code %d!\n",
				__func__,
				result);
	}
	if (enc10->base.ctx->dc->debug.hdmi20_disable)
		enc10->base.features.flags.bits.HDMI_6GB_EN = 0;

}

/* DPIA equivalent of link_transmitter_control. */
static bool link_dpia_control(struct dc_context *dc_ctx,
	struct dmub_cmd_dig_dpia_control_data *dpia_control)
{
	union dmub_rb_cmd cmd;

	memset(&cmd, 0, sizeof(cmd));

	cmd.dig1_dpia_control.header.type = DMUB_CMD__DPIA;
	cmd.dig1_dpia_control.header.sub_type =
			DMUB_CMD__DPIA_DIG1_DPIA_CONTROL;
	cmd.dig1_dpia_control.header.payload_bytes =
		sizeof(cmd.dig1_dpia_control) -
		sizeof(cmd.dig1_dpia_control.header);

	cmd.dig1_dpia_control.dpia_control = *dpia_control;

	dc_wake_and_execute_dmub_cmd(dc_ctx, &cmd, DM_DMUB_WAIT_TYPE_WAIT);

	return true;
}

static void link_encoder_disable(struct dcn10_link_encoder *enc10)
{
	/* reset training complete */
	REG_UPDATE(DP_LINK_CNTL, DP_LINK_TRAINING_COMPLETE, 0);
}

void dcn35_link_encoder_enable_dp_output(
	struct link_encoder *enc,
	const struct dc_link_settings *link_settings,
	enum clock_source_id clock_source)
{
	struct dcn10_link_encoder *enc10 = TO_DCN10_LINK_ENC(enc);

	if (!enc->ctx->dc->config.unify_link_enc_assignment)
		dcn31_link_encoder_enable_dp_output(enc, link_settings, clock_source);
	else {
		DC_LOG_DEBUG("%s: enc_id(%d)\n", __func__, enc->preferred_engine);
		dcn20_link_encoder_enable_dp_output(enc, link_settings, clock_source);
	}
}

void dcn35_link_encoder_enable_dp_mst_output(
	struct link_encoder *enc,
	const struct dc_link_settings *link_settings,
	enum clock_source_id clock_source)
{
	struct dcn10_link_encoder *enc10 = TO_DCN10_LINK_ENC(enc);

	if (!enc->ctx->dc->config.unify_link_enc_assignment)
		dcn31_link_encoder_enable_dp_mst_output(enc, link_settings, clock_source);
	else {
		DC_LOG_DEBUG("%s: enc_id(%d)\n", __func__, enc->preferred_engine);
		dcn10_link_encoder_enable_dp_mst_output(enc, link_settings, clock_source);
	}
}

void dcn35_link_encoder_disable_output(
	struct link_encoder *enc,
	enum signal_type signal)
{
	struct dcn10_link_encoder *enc10 = TO_DCN10_LINK_ENC(enc);

	if (!enc->ctx->dc->config.unify_link_enc_assignment)
		dcn31_link_encoder_disable_output(enc, signal);
	else {
		DC_LOG_DEBUG("%s: enc_id(%d)\n", __func__, enc->preferred_engine);
		dcn10_link_encoder_disable_output(enc, signal);
	}
}

void dcn35_link_encoder_enable_dpia_output(
	struct link_encoder *enc,
	const struct dc_link_settings *link_settings,
	uint8_t dpia_id,
	uint8_t digmode,
	uint8_t fec_rdy)
{
	struct dcn10_link_encoder *enc10 = TO_DCN10_LINK_ENC(enc);
	struct dmub_cmd_dig_dpia_control_data dpia_control = { 0 };

	enc1_configure_encoder(enc10, link_settings);

	dpia_control.action = (uint8_t)TRANSMITTER_CONTROL_ENABLE;
	dpia_control.enc_id = enc->preferred_engine;
	dpia_control.mode_laneset.digmode = digmode;
	dpia_control.lanenum = (uint8_t)link_settings->lane_count;
	dpia_control.symclk_10khz = link_settings->link_rate *
			LINK_RATE_REF_FREQ_IN_KHZ / 10;
	/* DIG_BE_CNTL.DIG_HPD_SELECT set to 5 (hpdsel - 1) to indicate HPD pin unused by DPIA. */
	dpia_control.hpdsel = 6;
	dpia_control.dpia_id = dpia_id;
	dpia_control.fec_rdy = fec_rdy;

	DC_LOG_DEBUG("%s: DPIA(%d) - enc_id(%d)\n", __func__, dpia_control.dpia_id, dpia_control.enc_id);
	link_dpia_control(enc->ctx, &dpia_control);
}

void dcn35_link_encoder_disable_dpia_output(
	struct link_encoder *enc,
	uint8_t dpia_id,
	uint8_t digmode)
{
	struct dcn10_link_encoder *enc10 = TO_DCN10_LINK_ENC(enc);
	struct dmub_cmd_dig_dpia_control_data dpia_control = { 0 };

	if (enc->funcs->is_dig_enabled && !enc->funcs->is_dig_enabled(enc))
		return;

	dpia_control.action = (uint8_t)TRANSMITTER_CONTROL_DISABLE;
	dpia_control.enc_id = enc->preferred_engine;
	dpia_control.mode_laneset.digmode = digmode;
	dpia_control.dpia_id = dpia_id;

	DC_LOG_DEBUG("%s: DPIA(%d) - enc_id(%d)\n", __func__, dpia_control.dpia_id, dpia_control.enc_id);
	link_dpia_control(enc->ctx, &dpia_control);

	link_encoder_disable(enc10);
}
