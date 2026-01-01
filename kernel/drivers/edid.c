#include <kernel/string.h>
#include <kernel/kheap.h>
#include <kernel/edid.h>
#include <kernel/trm.h>
#include <errno.h>

#define LOW(value) ((value) & 0xf)
#define HIGH(value) ((value >> 4) & 0xf)

static trm_timings_t established_modes[] = {
	// H-Disp, V-Disp, H-Total, V-Total, H-SyncStart, H-SyncEnd, V-SyncStart, V-SyncEnd, Clock(Hz), Refresh

	// established modes 1

	// Bit 0: 720 x 400 @ 70Hz
	{720, 400, 900, 449, 738, 846, 412, 414, 28322000, 70},

	// Bit 1: 720 x 400 @ 88Hz
	{720, 400, 936, 446, 738, 846, 411, 414, 35500000, 88},
 
	// Bit 2: 640 x 480 @ 60Hz
	{640, 480, 800, 525, 656, 752, 489, 491, 25175000, 60},

	// Bit 3: 640 x 480 @ 67Hz (Apple Macintosh)
	{640, 480, 864, 525, 704, 768, 483, 486, 30240000, 67},

	// Bit 4: 640 x 480 @ 72Hz
	{640, 480, 832, 520, 664, 704, 489, 492, 31500000, 72},

	// Bit 5: 640 x 480 @ 75Hz
	{640, 480, 840, 500, 656, 720, 481, 484, 31500000, 75},

	// Bit 6: 800 x 600 @ 56Hz
	{800, 600, 1024, 625, 832, 904, 601, 603, 36000000, 56},

	// Bit 7: 800 x 600 @ 60Hz
	{800, 600, 1056, 628, 840, 968, 601, 605, 40000000, 60}

	// established mode 2

	// Bit 8: 800 x 600 @ 72Hz
	{800,  600,  1040, 666,  856,  976,  637,  643,  50000000,  72},

	// Bit 9: 800 x 600 @ 75Hz
	{800,  600,  1056, 625,  816,  896,  601,  604,  49500000,  75},

	// Bit 10: 832 x 624 @ 75Hz (Apple Macintosh)
	{832,  624,  1152, 667,  864,  928,  625,  628,  57284000,  75},

	// Bit 11: 1024 x 768 @ 87Hz (Interlaced - IBM 8514 style)
	{1024, 768,  1264, 817,  1040, 1136, 770,  774,  44900000,  87},

	// Bit 12: 1024 x 768 @ 60Hz
	{1024, 768,  1344, 806,  1048, 1184, 771,  777,  65000000,  60},

	// Bit 13: 1024 x 768 @ 70Hz
	{1024, 768,  1328, 806,  1048, 1184, 771,  777,  75000000,  70},

	// Bit 14: 1024 x 768 @ 75Hz
	{1024, 768,  1312, 800,  1040, 1136, 769,  772,  78750000,  75},

	// Bit 15: 1280 x 1024 @ 75Hz
	{1280, 1024, 1688, 1066, 1296, 1440, 1025, 1028, 135000000, 75},
};

// Good old VGA 640 x 480 @ 60Hz as default
static trm_timings_t default_mode = {640, 480, 800, 525, 656, 752, 489, 491, 25175000, 60};

int edid_check(edid_t *edid) {
	uint8_t check = 0;
	uint8_t *buf = (uint8_t*)edid;
	for (size_t i=0; i<128; i++) {
		check += *buf;
		buf++;
	}
	return check == 0 ? 0 : -EINVAL;
}

static void add_mode(size_t *modes_count, trm_timings_t *modes, trm_timings_t *mode, trm_gpu_t *gpu) {
	// test if the gpu can accept this
	if (gpu) {
		// TODO: test
	}
	modes[*modes_count] = *mode;
	(*modes_count)++;
}

int edid_parse_connector(edid_t *edid, struct trm_connector *connector, struct trm_gpu *gpu) {
	if (edid_check(edid) < 0) {
		connector->type = TRM_CONNECTOR_UNDEF;
		connector->modes = kmalloc(sizeof(trm_timings_t));
		connector->modes_count = 1;
		connector->modes[0] = default_mode;
		return 0;
	}
	if (edid->input & EDID_INPUT_DIGITAL) {
		int edid_type = edid->input & EDID_DIGITAL_TYPE_MASK;
		switch (edid_type) {
		case EDID_DIGITAL_TYPE_DVI:
			connector->type = TRM_CONNECTOR_DVI;
			break;
		case EDID_DIGITAL_TYPE_HDMI_A:
		case EDID_DIGITAL_TYPE_HDMI_B:
			connector->type = TRM_CONNECTOR_HDMI;
			break;
		case EDID_DIGITAL_TYPE_MDDI:
			connector->type = TRM_CONNECTOR_MDDI;
			break;
		case EDID_DIGITAL_TYPE_DP:
			connector->type = TRM_CONNECTOR_DISPLAY_PORT;
			break;
		default:
			connector->type = TRM_CONNECTOR_UNDEF;
			break;
		}
	} else {
		// in theory it could be anolgic and not VGA (composite for exemple)
		// but who cares ?
		connector->type = TRM_CONNECTOR_VGA;
	}

	size_t modes_count;
	trm_timings_t modes[32];

	for (size_t i=0; i<4; i++) {
		edid_detailed_timing_t *desc = &edid->detailed_timings[i];
		if (desc->pixel_clock == 0) break;
		// we have to convert the mode
		trm_timing_t mode = {
			.hdisplay = desc->pixel->hactive_low | (HIGH(desc->pixel->hactive_hblank_high) << 8),
			.vdisplay = desc->pixel->vactive_low | (HIGH(desc->pixel->vactive_vblank_high) << 8),
			.pixel_clock = desc->pixel_clock * 10000,
		};
		uint16_t hblank = (desc->pixel->hblank_low | (LOW(desc->pixel->hactive_hblank_high) << 8));
		mode.htotal = mode.hdisplay + hblank;
		uint16_t vblank = (desc->pixel->vblank_low | (LOW(desc->pixel->vactive_vblank_high) << 8)); 
		mode.vtotal = mode.vdisplay + vblank;

		uint16_t hfront_proch = desc->pixel->hfront_porch_low | ((desc->pixel->hsync_vsync_front_porch_width_high << 2) & 0x300);
		mode.hsync_start = mode.hdisplay + hfront_porch;

		uint16_t vfront_proch = HIGH(desc->pixel->vfront_porch_vsync_width_low) | ((desc->pixel->hsync_vsync_front_porch_width_high << 2) & 0x30);
		mode.vsync_start = mode.vdisplay + vfront_porch;

		uint16_t hsync_width = desc->pixel->hsync_width_low | ((desc->pixel->hsync_vsync_front_porch_width_high << 4) & 0x300);
		mode.hsync_end = mode.hsync_start + hsync_width;

		uint16_t vsync_width = LOW(desc->pixel->vfront_porch_vsync_width_low) | ((desc->pixel->hsync_vsync_front_porch_width_high << 4) & 0x30);
		mode.vsync_end = mode.vsync_start + vsync_width;

		mode.refresh = mode.pixel_clock / (mode.htotal * mode.vtotal);

		add_mode(&mode_count, modes, &mode, gpu);
	}

	// parse established modes
	for (size_t i=0; i<16; i++) {
		uint8_t byte = i < 8 ? edid->established_timings1 : edid->established_timings2;
		if (byte & (1 << (i % 8))) {
			add_mode(&mode_count, modes, &established_modes[i], gpu);
		}
	}
	// TODO : parse more modes
	
	// if we have no mod fallback on default
	if (modes_count == 0) {
		add_mode(&mode_count, modes, &default_mode, gpu);
	}

	connector->modes_count = modes_count;
	connector->modes = kmalloc(sizeof(trm_timings_t) * modes_count);
	memcpy(connector->modes, modes, sizeof(trm_timings_t) * modes_count);
	return 0;
}
