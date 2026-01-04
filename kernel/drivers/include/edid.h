#ifndef _KERNEL_EDID_H
#define _KERNEL_EDID_H

// some macro are taken from DRM (drm_edid.h)

#include <stdint.h>

typedef struct edid_standard_timing {
	uint8_t hactive;
	uint8_t ar_refresh;
} edid_standard_timing_t;

typedef struct edid_detailed_timing_pixel {
	uint8_t hactive_low;
	uint8_t hblank_low;
	uint8_t hactive_hblank_high;
	uint8_t vactive_low;
	uint8_t vblank_low;
	uint8_t vactive_vblank_high;
	uint8_t hfront_porch_low;
	uint8_t hsync_width_low;
	uint8_t vfront_porch_vsync_width_low;
	uint8_t hsync_vsync_front_porch_width_high;
	uint8_t width_mm_low;
	uint8_t heigh_mm_low;
	uint8_t width_height_mm_low;
	uint8_t hborder;
	uint8_t vborder;
	uint8_t misc;
} edid_detailed_timing_pixel_t;

typedef struct edid_display_descriptor {
	uint8_t reserved0;
	uint8_t tag;
	uint8_t reserved1;
	// TODO
} edid_display_descriptor_t;

typedef struct edid_detailed_timing {
	uint16_t pixel_clock; // if 0 is is a display dsec
	union {
		edid_detailed_timing_pixel_t pixel;
		edid_display_descriptor_t descriptor;
	};
} edid_detailed_timing_t;

typedef struct edid {
	uint8_t magic[8];

	// product
	uint16_t manufacturer;
	uint16_t product_code;
	uint32_t serial_code;
	uint8_t manufacture_week;
	uint8_t manufacture_year;

	// EDID version
	uint8_t edid_version;
	uint8_t edid_revision;

	// basic display features
	uint8_t input;
	uint8_t width_cm;
	uint8_t height_cm;
	uint8_t display_transfer;
	uint8_t features;

	// color stats
	uint8_t rg_low;
	uint8_t bw_low;
	uint8_t red_x;
	uint8_t red_y;
	uint8_t green_x;
	uint8_t green_y;
	uint8_t blue_x;
	uint8_t blue_y;
	uint8_t white_x;
	uint8_t white_y;

	// ethablished timings
	uint8_t established_timings1;
	uint8_t established_timings2;
	uint8_t manufacturer_timings1;

	// standard timings
	edid_standard_timing_t standard_timings[8];

	// data block
	edid_detailed_timing_t detailed_timings[4];

	// extention block count
	uint8_t extention_blocks_count;

	// checksum
	uint8_t checksum;

} edid_t;

// input field
#define EDID_INPUT_SERRATION_VSYNC (1 << 0)
#define EDID_INPUT_SYNC_ON_GREEN   (1 << 1)
#define EDID_INPUT_COMPOSITE_SYNC  (1 << 2)
#define EDID_INPUT_SEPARATE_SYNCS  (1 << 3)
#define EDID_INPUT_BLANK_TO_BLACK  (1 << 4)
#define EDID_INPUT_VIDEO_LEVEL     (3 << 5)
#define EDID_INPUT_DIGITAL         (1 << 7)
#define EDID_DIGITAL_DEPTH_MASK    (7 << 4) /* 1.4 */
#define EDID_DIGITAL_DEPTH_UNDEF   (0 << 4) /* 1.4 */
#define EDID_DIGITAL_DEPTH_6       (1 << 4) /* 1.4 */
#define EDID_DIGITAL_DEPTH_8       (2 << 4) /* 1.4 */
#define EDID_DIGITAL_DEPTH_10      (3 << 4) /* 1.4 */
#define EDID_DIGITAL_DEPTH_12      (4 << 4) /* 1.4 */
#define EDID_DIGITAL_DEPTH_14      (5 << 4) /* 1.4 */
#define EDID_DIGITAL_DEPTH_16      (6 << 4) /* 1.4 */
#define EDID_DIGITAL_DEPTH_RSVD    (7 << 4) /* 1.4 */
#define EDID_DIGITAL_TYPE_MASK     (7 << 0) /* 1.4 */
#define EDID_DIGITAL_TYPE_UNDEF    (0 << 0) /* 1.4 */
#define EDID_DIGITAL_TYPE_DVI      (1 << 0) /* 1.4 */
#define EDID_DIGITAL_TYPE_HDMI_A   (2 << 0) /* 1.4 */
#define EDID_DIGITAL_TYPE_HDMI_B   (3 << 0) /* 1.4 */
#define EDID_DIGITAL_TYPE_MDDI     (4 << 0) /* 1.4 */
#define EDID_DIGITAL_TYPE_DP       (5 << 0) /* 1.4 */
#define EDID_DIGITAL_DFP_1_X       (1 << 0) /* 1.3 */

// features field
#define DRM_EDID_FEATURE_DEFAULT_GTF      (1 << 0) /* 1.2 */
#define DRM_EDID_FEATURE_CONTINUOUS_FREQ  (1 << 0) /* 1.4 */
#define DRM_EDID_FEATURE_PREFERRED_TIMING (1 << 1)
#define DRM_EDID_FEATURE_STANDARD_COLOR   (1 << 2)
/* If analog */
#define DRM_EDID_FEATURE_DISPLAY_TYPE     (3 << 3) /* 00=mono, 01=rgb, 10=non-rgb, 11=unknown */
/* If digital */
#define DRM_EDID_FEATURE_COLOR_MASK	  (3 << 3)
#define DRM_EDID_FEATURE_RGB		  (0 << 3)
#define DRM_EDID_FEATURE_RGB_YCRCB444	  (1 << 3)
#define DRM_EDID_FEATURE_RGB_YCRCB422	  (2 << 3)
#define DRM_EDID_FEATURE_RGB_YCRCB	  (3 << 3) /* both 4:4:4 and 4:2:2 */

#define DRM_EDID_FEATURE_PM_ACTIVE_OFF    (1 << 5)
#define DRM_EDID_FEATURE_PM_SUSPEND       (1 << 6)
#define DRM_EDID_FEATURE_PM_STANDBY       (1 << 7)

struct trm_connector;
struct trm_gpu;

// functions
int edid_check(edid_t *edid);
int edid_parse_connector(edid_t *edid, struct trm_connector *, struct trm_gpu *gpu);

#endif
