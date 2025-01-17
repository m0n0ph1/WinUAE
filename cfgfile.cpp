/*
* UAE - The Un*x Amiga Emulator
*
* Config file handling
* This still needs some thought before it's complete...
*
* Copyright 1998 Brian King, Bernd Schmidt
*/

#include "sysconfig.h"
#include "sysdeps.h"

#include <ctype.h>

#include "options.h"
#include "uae.h"
#include "audio.h"
#include "events.h"
#include "custom.h"
#include "inputdevice.h"
#include "gfxfilter.h"
#include "savestate.h"
#include "memory.h"
#include "autoconf.h"
#include "rommgr.h"
#include "gui.h"
#include "newcpu.h"
#include "zfile.h"
#include "filesys.h"
#include "fsdb.h"
#include "disk.h"
#include "blkdev.h"
#include "statusline.h"
#include "debug.h"
#include "calc.h"
#include "gfxboard.h"
#include "cpuboard.h"
#include "luascript.h"

static int config_newfilesystem;
static struct strlist *temp_lines;
static struct strlist *error_lines;
static struct zfile *default_file, *configstore;
static int uaeconfig;
static int unicode_config = 0;

/* @@@ need to get rid of this... just cut part of the manual and print that
* as a help text.  */
struct cfg_lines
{
	const TCHAR *config_label, *config_help;
};

static const struct cfg_lines opttable[] =
{
	{_T("help"), _T("Prints this help") },
	{_T("config_description"), _T("") },
	{_T("config_info"), _T("") },
	{_T("use_gui"), _T("Enable the GUI?  If no, then goes straight to emulator") },
	{_T("use_debugger"), _T("Enable the debugger?") },
	{_T("cpu_speed"), _T("can be max, real, or a number between 1 and 20") },
	{_T("cpu_model"), _T("Can be 68000, 68010, 68020, 68030, 68040, 68060") },
	{_T("fpu_model"), _T("Can be 68881, 68882, 68040, 68060") },
	{_T("cpu_compatible"), _T("yes enables compatibility-mode") },
	{_T("cpu_24bit_addressing"), _T("must be set to 'no' in order for Z3mem or P96mem to work") },
	{_T("autoconfig"), _T("yes = add filesystems and extra ram") },
	{_T("log_illegal_mem"), _T("print illegal memory access by Amiga software?") },
	{_T("fastmem_size"), _T("Size in megabytes of fast-memory") },
	{_T("chipmem_size"), _T("Size in megabytes of chip-memory") },
	{_T("bogomem_size"), _T("Size in megabytes of bogo-memory at 0xC00000") },
	{_T("a3000mem_size"), _T("Size in megabytes of A3000 memory") },
	{_T("gfxcard_size"), _T("Size in megabytes of Picasso96 graphics-card memory") },
	{_T("z3mem_size"), _T("Size in megabytes of Zorro-III expansion memory") },
	{_T("gfx_test_speed"), _T("Test graphics speed?") },
	{_T("gfx_framerate"), _T("Print every nth frame") },
	{_T("gfx_width"), _T("Screen width") },
	{_T("gfx_height"), _T("Screen height") },
	{_T("gfx_refreshrate"), _T("Fullscreen refresh rate") },
	{_T("gfx_vsync"), _T("Sync screen refresh to refresh rate") },
	{_T("gfx_lores"), _T("Treat display as lo-res?") },
	{_T("gfx_linemode"), _T("Can be none, double, or scanlines") },
	{_T("gfx_fullscreen_amiga"), _T("Amiga screens are fullscreen?") },
	{_T("gfx_fullscreen_picasso"), _T("Picasso screens are fullscreen?") },
	{_T("gfx_center_horizontal"), _T("Center display horizontally?") },
	{_T("gfx_center_vertical"), _T("Center display vertically?") },
	{_T("gfx_colour_mode"), _T("") },
	{_T("32bit_blits"), _T("Enable 32 bit blitter emulation") },
	{_T("immediate_blits"), _T("Perform blits immediately") },
	{_T("show_leds"), _T("LED display") },
	{_T("keyboard_leds"), _T("Keyboard LEDs") },
	{_T("gfxlib_replacement"), _T("Use graphics.library replacement?") },
	{_T("sound_output"), _T("") },
	{_T("sound_frequency"), _T("") },
	{_T("sound_bits"), _T("") },
	{_T("sound_channels"), _T("") },
	{_T("sound_max_buff"), _T("") },
	{_T("comp_trustbyte"), _T("How to access bytes in compiler (direct/indirect/indirectKS/afterPic") },
	{_T("comp_trustword"), _T("How to access words in compiler (direct/indirect/indirectKS/afterPic") },
	{_T("comp_trustlong"), _T("How to access longs in compiler (direct/indirect/indirectKS/afterPic") },
	{_T("comp_nf"), _T("Whether to optimize away flag generation where possible") },
	{_T("comp_fpu"), _T("Whether to provide JIT FPU emulation") },
	{_T("compforcesettings"), _T("Whether to force the JIT compiler settings") },
	{_T("cachesize"), _T("How many MB to use to buffer translated instructions")},
	{_T("override_dga_address"),_T("Address from which to map the frame buffer (upper 16 bits) (DANGEROUS!)")},
	{_T("avoid_cmov"), _T("Set to yes on machines that lack the CMOV instruction") },
	{_T("avoid_dga"), _T("Set to yes if the use of DGA extension creates problems") },
	{_T("avoid_vid"), _T("Set to yes if the use of the Vidmode extension creates problems") },
	{_T("parallel_on_demand"), _T("") },
	{_T("serial_on_demand"), _T("") },
	{_T("scsi"), _T("scsi.device emulation") },
	{_T("joyport0"), _T("") },
	{_T("joyport1"), _T("") },
	{_T("pci_devices"), _T("List of PCI devices to make visible to the emulated Amiga") },
	{_T("kickstart_rom_file"), _T("Kickstart ROM image, (C) Copyright Amiga, Inc.") },
	{_T("kickstart_ext_rom_file"), _T("Extended Kickstart ROM image, (C) Copyright Amiga, Inc.") },
	{_T("kickstart_key_file"), _T("Key-file for encrypted ROM images (from Cloanto's Amiga Forever)") },
	{_T("flash_ram_file"), _T("Flash/battery backed RAM image file.") },
	{_T("cart_file"), _T("Freezer cartridge ROM image file.") },
	{_T("floppy0"), _T("Diskfile for drive 0") },
	{_T("floppy1"), _T("Diskfile for drive 1") },
	{_T("floppy2"), _T("Diskfile for drive 2") },
	{_T("floppy3"), _T("Diskfile for drive 3") },
	{_T("hardfile"), _T("access,sectors, surfaces, reserved, blocksize, path format") },
	{_T("filesystem"), _T("access,'Amiga volume-name':'host directory path' - where 'access' can be 'read-only' or 'read-write'") },
	{_T("catweasel"), _T("Catweasel board io base address") }
};

static const TCHAR *guimode1[] = { _T("no"), _T("yes"), _T("nowait"), 0 };
static const TCHAR *guimode2[] = { _T("false"), _T("true"), _T("nowait"), 0 };
static const TCHAR *guimode3[] = { _T("0"), _T("1"), _T("nowait"), 0 };
static const TCHAR *csmode[] = { _T("ocs"), _T("ecs_agnus"), _T("ecs_denise"), _T("ecs"), _T("aga"), 0 };
static const TCHAR *linemode[] = {
	_T("none"),
	_T("double"), _T("scanlines"), _T("scanlines2p"), _T("scanlines3p"),
	_T("double2"), _T("scanlines2"), _T("scanlines2p2"), _T("scanlines2p3"),
	_T("double3"), _T("scanlines3"), _T("scanlines3p2"), _T("scanlines3p3"),
	0 };
static const TCHAR *speedmode[] = { _T("max"), _T("real"), 0 };
static const TCHAR *colormode1[] = { _T("8bit"), _T("15bit"), _T("16bit"), _T("8bit_dither"), _T("4bit_dither"), _T("32bit"), 0 };
static const TCHAR *colormode2[] = { _T("8"), _T("15"), _T("16"), _T("8d"), _T("4d"), _T("32"), 0 };
static const TCHAR *soundmode1[] = { _T("none"), _T("interrupts"), _T("normal"), _T("exact"), 0 };
static const TCHAR *soundmode2[] = { _T("none"), _T("interrupts"), _T("good"), _T("best"), 0 };
static const TCHAR *centermode1[] = { _T("none"), _T("simple"), _T("smart"), 0 };
static const TCHAR *centermode2[] = { _T("false"), _T("true"), _T("smart"), 0 };
static const TCHAR *stereomode[] = { _T("mono"), _T("stereo"), _T("clonedstereo"), _T("4ch"), _T("clonedstereo6ch"), _T("6ch"), _T("mixed"), 0 };
static const TCHAR *interpolmode[] = { _T("none"), _T("anti"), _T("sinc"), _T("rh"), _T("crux"), 0 };
static const TCHAR *collmode[] = { _T("none"), _T("sprites"), _T("playfields"), _T("full"), 0 };
static const TCHAR *compmode[] = { _T("direct"), _T("indirect"), _T("indirectKS"), _T("afterPic"), 0 };
static const TCHAR *flushmode[] = { _T("soft"), _T("hard"), 0 };
static const TCHAR *kbleds[] = { _T("none"), _T("POWER"), _T("DF0"), _T("DF1"), _T("DF2"), _T("DF3"), _T("HD"), _T("CD"), _T("DFx"), 0 };
static const TCHAR *onscreenleds[] = { _T("false"), _T("true"), _T("rtg"), _T("both"), 0 };
static const TCHAR *soundfiltermode1[] = { _T("off"), _T("emulated"), _T("on"), 0 };
static const TCHAR *soundfiltermode2[] = { _T("standard"), _T("enhanced"), 0 };
static const TCHAR *lorestype1[] = { _T("lores"), _T("hires"), _T("superhires"), 0 };
static const TCHAR *lorestype2[] = { _T("true"), _T("false"), 0 };
static const TCHAR *loresmode[] = { _T("normal"), _T("filtered"), 0 };
static const TCHAR *horizmode[] = { _T("vertical"), _T("lores"), _T("hires"), _T("superhires"), 0 };
static const TCHAR *vertmode[] = { _T("horizontal"), _T("single"), _T("double"), _T("quadruple"), 0 };
#ifdef GFXFILTER
static const TCHAR *filtermode2[] = { _T("1x"), _T("2x"), _T("3x"), _T("4x"), 0 };
#endif
static const TCHAR *cartsmode[] = { _T("none"), _T("hrtmon"), 0 };
static const TCHAR *idemode[] = { _T("none"), _T("a600/a1200"), _T("a4000"), 0 };
static const TCHAR *rtctype[] = { _T("none"), _T("MSM6242B"), _T("RP5C01A"), _T("MSM6242B_A2000"), 0 };
static const TCHAR *ciaatodmode[] = { _T("vblank"), _T("50hz"), _T("60hz"), 0 };
static const TCHAR *ksmirrortype[] = { _T("none"), _T("e0"), _T("a8+e0"), 0 };
static const TCHAR *cscompa[] = {
	_T("-"), _T("Generic"), _T("CDTV"), _T("CDTV-CR"), _T("CD32"), _T("A500"), _T("A500+"), _T("A600"),
	_T("A1000"), _T("A1200"), _T("A2000"), _T("A3000"), _T("A3000T"), _T("A4000"), _T("A4000T"),
	_T("Velvet"),
	NULL
};
static const TCHAR *qsmodes[] = {
	_T("A500"), _T("A500+"), _T("A600"), _T("A1000"), _T("A1200"), _T("A3000"), _T("A4000"), _T(""), _T("CD32"), _T("CDTV"), _T("CDTV-CR"), _T("ARCADIA"), NULL };
/* 3-state boolean! */
static const TCHAR *fullmodes[] = { _T("false"), _T("true"), /* "FILE_NOT_FOUND", */ _T("fullwindow"), 0 };
/* bleh for compatibility */
static const TCHAR *scsimode[] = { _T("false"), _T("true"), _T("scsi"), 0 };
static const TCHAR *maxhoriz[] = { _T("lores"), _T("hires"), _T("superhires"), 0 };
static const TCHAR *maxvert[] = { _T("nointerlace"), _T("interlace"), 0 };
static const TCHAR *abspointers[] = { _T("none"), _T("mousehack"), _T("tablet"), 0 };
static const TCHAR *magiccursors[] = { _T("both"), _T("native"), _T("host"), 0 };
static const TCHAR *autoscale[] = { _T("none"), _T("auto"), _T("standard"), _T("max"), _T("scale"), _T("resize"), _T("center"), _T("manual"),
	_T("integer"), _T("integer_auto"), _T("separator"), _T("overscan_blanking"), 0 };
static const TCHAR *autoscale_rtg[] = { _T("resize"), _T("scale"), _T("center"), _T("integer"), 0 };
static const TCHAR *autoscalelimit[] = { _T("1/1"), _T("1/2"), _T("1/4"), _T("1/8"), 0 };
static const TCHAR *joyportmodes[] = { _T(""), _T("mouse"), _T("mousenowheel"), _T("djoy"), _T("gamepad"), _T("ajoy"), _T("cdtvjoy"), _T("cd32joy"), _T("lightpen"), 0 };
static const TCHAR *joyaf[] = { _T("none"), _T("normal"), _T("toggle"), _T("always"), 0 };
static const TCHAR *epsonprinter[] = { _T("none"), _T("ascii"), _T("epson_matrix_9pin"), _T("epson_matrix_24pin"), _T("epson_matrix_48pin"), 0 };
static const TCHAR *aspects[] = { _T("none"), _T("vga"), _T("tv"), 0 };
static const TCHAR *vsyncmodes[] = { _T("false"), _T("true"), _T("autoswitch"), 0 };
static const TCHAR *vsyncmodes2[] = { _T("normal"), _T("busywait"), 0 };
static const TCHAR *filterapi[] = { _T("directdraw"), _T("direct3d"), 0 };
static const TCHAR *dongles[] =
{
	_T("none"),
	_T("robocop 3"), _T("leaderboard"), _T("b.a.t. ii"), _T("italy'90 soccer"), _T("dames grand maitre"),
	_T("rugby coach"), _T("cricket captain"), _T("leviathan"),
	NULL
};
static const TCHAR *cdmodes[] = { _T("disabled"), _T(""), _T("image"), _T("ioctl"), _T("spti"), _T("aspi"), 0 };
static const TCHAR *cdconmodes[] = { _T(""), _T("uae"), _T("ide"), _T("scsi"), _T("cdtv"), _T("cd32"), 0 };
static const TCHAR *specialmonitors[] = { _T("none"), _T("autodetect"), _T("a2024"), _T("graffiti"),
_T("ham_e"), _T("ham_e_plus"), _T("videodac18"), _T("avideo12"), _T("avideo24"), _T("firecracker24"), _T("dctv"), 0 };
static const TCHAR *genlockmodes[] = { _T("none"), _T("noise"), _T("testcard"), NULL };
static const TCHAR *ppc_implementations[] = {
	_T("auto"),
	_T("dummy"),
	_T("pearpc"),
	_T("qemu"),
	NULL
};
static const TCHAR *ppc_cpu_idle[] = {
	_T("disabled"),
	_T("1"),
	_T("2"),
	_T("3"),
	_T("4"),
	_T("5"),
	_T("6"),
	_T("7"),
	_T("8"),
	_T("9"),
	_T("max"),
	NULL
};
static const TCHAR *waitblits[] = { _T("disabled"), _T("automatic"), _T("noidleonly"), _T("always"), 0 };
static const TCHAR *autoext2[] = { _T("disabled"), _T("copy"), _T("replace"), 0 };
static const TCHAR *leds[] = { _T("power"), _T("df0"), _T("df1"), _T("df2"), _T("df3"), _T("hd"), _T("cd"), _T("fps"), _T("cpu"), _T("snd"), _T("md"), 0 };
static const int leds_order[] = { 3, 6, 7, 8, 9, 4, 5, 2, 1, 0, 9 };
static const TCHAR *lacer[] = { _T("off"), _T("i"), _T("p"), 0 };

static const TCHAR *hdcontrollers[] = {
	_T("uae"),

	_T("ide%d"),
	_T("ide%d_mainboard"),

	_T("scsi%d"),
	_T("scsi%d_a3000"),
	_T("scsi%d_a4000t"),
	_T("scsi%d_cdtv"),

	_T("scsram"),
	_T("scide"),
	NULL
};
static const TCHAR *z3mapping[] = {
	_T("auto"),
	_T("uae"),
	_T("real"),
	NULL
};
static const TCHAR *uaescsidevmodes[] = {
	_T("original"),
	_T("rename_scsi"),
	NULL
};
static const TCHAR *uaebootrom[] = {
	_T("automatic"),
	_T("disabled"), 
	_T("min"),
	_T("full"),
	NULL
};
static const TCHAR *serialcrlf[] = {
	_T("disabled"),
	_T("crlf_cr"),
	NULL
};
static const TCHAR *obsolete[] = {
	_T("accuracy"), _T("gfx_opengl"), _T("gfx_32bit_blits"), _T("32bit_blits"),
	_T("gfx_immediate_blits"), _T("gfx_ntsc"), _T("win32"), _T("gfx_filter_bits"),
	_T("sound_pri_cutoff"), _T("sound_pri_time"), _T("sound_min_buff"), _T("sound_bits"),
	_T("gfx_test_speed"), _T("gfxlib_replacement"), _T("enforcer"), _T("catweasel_io"),
	_T("kickstart_key_file"), _T("fast_copper"), _T("sound_adjust"), _T("sound_latency"),
	_T("serial_hardware_dtrdsr"), _T("gfx_filter_upscale"),
	_T("gfx_correct_aspect"), _T("gfx_autoscale"), _T("parallel_sampler"), _T("parallel_ascii_emulation"),
	_T("avoid_vid"), _T("avoid_dga"), _T("z3chipmem_size"), _T("state_replay_buffer"), _T("state_replay"),
	_T("z3realmapping"), _T("force_0x10000000_z3"),
	
	_T("gfx_filter_vert_zoom"),_T("gfx_filter_horiz_zoom"),
	_T("gfx_filter_vert_zoom_mult"), _T("gfx_filter_horiz_zoom_mult"),
	_T("gfx_filter_vert_offset"), _T("gfx_filter_horiz_offset"),
	_T("rtg_vert_zoom_multf"), _T("rtg_horiz_zoom_multf"),
	
	// created by some buggy beta
	_T("uaehf0%s,%s"),
	_T("uaehf1%s,%s"),
	_T("uaehf2%s,%s"),
	_T("uaehf3%s,%s"),
	_T("uaehf4%s,%s"),
	_T("uaehf5%s,%s"),
	_T("uaehf6%s,%s"),
	_T("uaehf7%s,%s"),

	_T("pcibridge_rom_file"),
	_T("pcibridge_rom_options"),

	_T("cpuboard_ext_rom_file"),

	NULL
};

#define UNEXPANDED _T("$(FILE_PATH)")


static void trimwsa (char *s)
{
	/* Delete trailing whitespace.  */
	int len = strlen (s);
	while (len > 0 && strcspn (s + len - 1, "\t \r\n") == 0)
		s[--len] = '\0';
}

static int match_string (const TCHAR *table[], const TCHAR *str)
{
	int i;
	for (i = 0; table[i] != 0; i++)
		if (strcasecmp (table[i], str) == 0)
			return i;
	return -1;
}

// escape config file separators and control characters
static TCHAR *cfgfile_escape (const TCHAR *s, const TCHAR *escstr, bool quote)
{
	bool doquote = false;
	int cnt = 0;
	for (int i = 0; s[i]; i++) {
		TCHAR c = s[i];
		if (c == 0)
			break;
		if (c < 32 || c == '\\' || c == '\"' || c == '\'') {
			cnt++;
		}
		for (int j = 0; escstr && escstr[j]; j++) {
			if (c == escstr[j]) {
				cnt++;
				if (quote) {
					doquote = true;
					cnt++;
				}
			}
		}
	}
	TCHAR *s2 = xmalloc (TCHAR, _tcslen (s) + cnt * 4 + 1);
	TCHAR *p = s2;
	if (doquote)
		*p++ = '\"';
	for (int i = 0; s[i]; i++) {
		TCHAR c = s[i];
		if (c == 0)
			break;
		if (c == '\\' || c == '\"' || c == '\'') {
			*p++ = '\\';
			*p++ = c;
		} else if (c >= 32 && !quote) {
			bool escaped = false;
			for (int j = 0; escstr && escstr[j]; j++) {
				if (c == escstr[j]) {
					*p++ = '\\';
					*p++ = c;
					escaped = true;
					break;
				}
			}
			if (!escaped)
				*p++ = c;
		} else if (c < 32) {
			*p++ = '\\';
			switch (c)
			{
				case '\t':
				*p++ = 't';
				break;
				case '\n':
				*p++ = 'n';
				break;
				case '\r':
				*p++ = 'r';
				break;
				default:
				*p++ = 'x';
				*p++ = (c >> 4) >= 10 ? (c >> 4) + 'a' : (c >> 4) + '0';
				*p++ = (c & 15) >= 10 ? (c & 15) + 'a' : (c & 15) + '0';
				break;
			}
		} else {
			*p++ = c;
		}
	}
	if (doquote)
		*p++ = '\"';
	*p = 0;
	return s2;
}
static TCHAR *cfgfile_unescape (const TCHAR *s, const TCHAR **endpos, TCHAR separator)
{
	bool quoted = false;
	TCHAR *s2 = xmalloc (TCHAR, _tcslen (s) + 1);
	TCHAR *p = s2;
	if (s[0] == '\"') {
		s++;
		quoted = true;
	}
	int i;
	for (i = 0; s[i]; i++) {
		TCHAR c = s[i];
		if (quoted && c == '\"') {
			i++;
			break;
		}
		if (c == separator) {
			i++;
			break;
		}
		if (c == '\\') {
			char v = 0;
			TCHAR c2;
			c = s[i + 1];
			switch (c)
			{
				case 'X':
				case 'x':
				c2 = _totupper (s[i + 2]);
				v = ((c2 >= 'A') ? c2 - 'A' : c2 - '0') << 4;
				c2 = _totupper (s[i + 3]);
				v |= (c2 >= 'A') ? c2 - 'A' : c2 - '0';
				*p++ = c2;
				i += 2;
				break;
				case 'r':
				*p++ = '\r';
				break;
				case '\n':
				*p++ = '\n';
				break;
				default:
				*p++ = c;
				break;
			}
			i++;
		} else {
			*p++ = c;
		}
	}
	*p = 0;
	if (endpos)
		*endpos = &s[i];
	return s2;
}
static TCHAR *cfgfile_unescape (const TCHAR *s, const TCHAR **endpos)
{
	return cfgfile_unescape (s, endpos, 0);
}

static TCHAR *getnextentry (const TCHAR **valuep, const TCHAR separator)
{
	TCHAR *s;
	const TCHAR *value = *valuep;
	if (value[0] == '\"') {
		s = cfgfile_unescape (value, valuep);
		value = *valuep;
		if (*value != 0 && *value != separator) {
			xfree (s);
			return NULL;
		}
		value++;
		*valuep = value;
	} else {
		s = cfgfile_unescape (value, valuep, separator);
	}
	return s;
}

static TCHAR *cfgfile_subst_path2 (const TCHAR *path, const TCHAR *subst, const TCHAR *file)
{
	/* @@@ use strcasecmp for some targets.  */
	if (_tcslen (path) > 0 && _tcsncmp (file, path, _tcslen (path)) == 0) {
		int l;
		TCHAR *p2, *p = xmalloc (TCHAR, _tcslen (file) + _tcslen (subst) + 2);
		_tcscpy (p, subst);
		l = _tcslen (p);
		while (l > 0 && p[l - 1] == '/')
			p[--l] = '\0';
		l = _tcslen (path);
		while (file[l] == '/')
			l++;
		_tcscat (p, _T("/"));
		_tcscat (p, file + l);
		p2 = target_expand_environment (p);
		xfree (p);
		return p2;
	}
	return NULL;
}

TCHAR *cfgfile_subst_path (const TCHAR *path, const TCHAR *subst, const TCHAR *file)
{
	TCHAR *s = cfgfile_subst_path2 (path, subst, file);
	if (s)
		return s;
	s = target_expand_environment (file);
	if (s) {
		TCHAR tmp[MAX_DPATH];
		_tcscpy (tmp, s);
		xfree (s);
		fullpath (tmp, sizeof tmp / sizeof (TCHAR));
		s = my_strdup (tmp);
	}
	return s;
}

static TCHAR *cfgfile_get_multipath2 (struct multipath *mp, const TCHAR *path, const TCHAR *file, bool dir)
{
	for (int i = 0; i < MAX_PATHS; i++) {
		if (mp->path[i][0] && _tcscmp (mp->path[i], _T(".\\")) != 0 && _tcscmp (mp->path[i], _T("./")) != 0 && (file[0] != '/' && file[0] != '\\' && !_tcschr(file, ':'))) {
			TCHAR *s = NULL;
			if (path)
				s = cfgfile_subst_path2 (path, mp->path[i], file);
			if (!s) {
				TCHAR np[MAX_DPATH];
				_tcscpy (np, mp->path[i]);
				fixtrailing (np);
				_tcscat (np, file);
				fullpath (np, sizeof np / sizeof (TCHAR));
				s = my_strdup (np);
			}
			if (dir) {
				if (my_existsdir (s))
					return s;
			} else {
				if (zfile_exists (s))
					return s;
			}
			xfree (s);
		}
	}
	return NULL;
}

static TCHAR *cfgfile_get_multipath (struct multipath *mp, const TCHAR *path, const TCHAR *file, bool dir)
{
	TCHAR *s = cfgfile_get_multipath2 (mp, path, file, dir);
	if (s)
		return s;
	return my_strdup (file);
}

static TCHAR *cfgfile_put_multipath (struct multipath *mp, const TCHAR *s)
{
	for (int i = 0; i < MAX_PATHS; i++) {
		if (mp->path[i][0] && _tcscmp (mp->path[i], _T(".\\")) != 0 && _tcscmp (mp->path[i], _T("./")) != 0) {
			if (_tcsnicmp (mp->path[i], s, _tcslen (mp->path[i])) == 0) {
				return my_strdup (s + _tcslen (mp->path[i]));
			}
		}
	}
	return my_strdup (s);
}

static TCHAR *cfgfile_subst_path_load (const TCHAR *path, struct multipath *mp, const TCHAR *file, bool dir)
{
	TCHAR *s = cfgfile_get_multipath2 (mp, path, file, dir);
	if (s)
		return s;
	return cfgfile_subst_path (path, mp->path[0], file);
}

static bool isdefault (const TCHAR *s)
{
	TCHAR tmp[MAX_DPATH];
	if (!default_file || uaeconfig)
		return false;
	zfile_fseek (default_file, 0, SEEK_SET);
	while (zfile_fgets (tmp, sizeof tmp / sizeof (TCHAR), default_file)) {
		if (tmp[0] && tmp[_tcslen (tmp) - 1] == '\n')
			tmp[_tcslen (tmp) - 1] = 0;
		if (!_tcscmp (tmp, s))
			return true;
	}
	return false;
}

static size_t cfg_write (const void *b, struct zfile *z)
{
	size_t v;
	if (unicode_config) {
		TCHAR lf = 10;
		v = zfile_fwrite (b, _tcslen ((TCHAR*)b), sizeof (TCHAR), z);
		zfile_fwrite (&lf, 1, 1, z);
	} else {
		char lf = 10;
		char *s = ua ((TCHAR*)b);
		v = zfile_fwrite (s, strlen (s), 1, z);
		zfile_fwrite (&lf, 1, 1, z);
		xfree (s);
	}
	return v;
}

#define UTF8NAME _T(".utf8")

static void cfg_dowrite (struct zfile *f, const TCHAR *option, const TCHAR *optionext, const TCHAR *value, int d, int target)
{
	char lf = 10;
	TCHAR tmp[CONFIG_BLEN], tmpext[CONFIG_BLEN];
	const TCHAR *optionp;
	char tmpa[CONFIG_BLEN];
	char *tmp1, *tmp2;
	int utf8;

	if (value == NULL)
		return;
	if (optionext) {
		_tcscpy (tmpext, option);
		_tcscat (tmpext, optionext);
		optionp = tmpext;
	} else {
		optionp = option;
	}
	utf8 = 0;
	tmp1 = ua (value);
	tmp2 = uutf8 (value);
	if (strcmp (tmp1, tmp2) && tmp2[0] != 0)
		utf8 = 1;

	if (target)
		_stprintf (tmp, _T("%s.%s=%s"), TARGET_NAME, optionp, value);
	else
		_stprintf (tmp, _T("%s=%s"), optionp, value);
	if (d && isdefault (tmp))
		goto end;
	cfg_write (tmp, f);
	if (utf8 && !unicode_config) {
		char *opt = ua (optionp);
		if (target) {
			char *tna = ua (TARGET_NAME);
			sprintf (tmpa, "%s.%s.utf8=%s", tna, opt, tmp2);
			xfree (tna);
		} else {
			sprintf (tmpa, "%s.utf8=%s", opt, tmp2);
		}
		xfree (opt);
		zfile_fwrite (tmpa, strlen (tmpa), 1, f);
		zfile_fwrite (&lf, 1, 1, f);
	}
end:
	xfree (tmp2);
	xfree (tmp1);
}
static void cfg_dowrite (struct zfile *f, const TCHAR *option, const TCHAR *value, int d, int target)
{
	cfg_dowrite (f, option, NULL, value, d, target);
}
void cfgfile_write_bool (struct zfile *f, const TCHAR *option, bool b)
{
	cfg_dowrite (f, option, b ? _T("true") : _T("false"), 0, 0);
}
void cfgfile_dwrite_bool (struct zfile *f, const TCHAR *option, bool b)
{
	cfg_dowrite (f, option, b ? _T("true") : _T("false"), 1, 0);
}
void cfgfile_dwrite_bool (struct zfile *f, const TCHAR *option, const TCHAR *optionext, bool b)
{
	cfg_dowrite (f, option, optionext, b ? _T("true") : _T("false"), 1, 0);
}
void cfgfile_dwrite_bool (struct zfile *f, const TCHAR *option, int b)
{
	cfgfile_dwrite_bool (f, option, b != 0);
}
void cfgfile_write_str (struct zfile *f, const TCHAR *option, const TCHAR *value)
{
	cfg_dowrite (f, option, value, 0, 0);
}
void cfgfile_write_str (struct zfile *f, const TCHAR *option, const TCHAR *optionext, const TCHAR *value)
{
	cfg_dowrite (f, option, optionext, value, 0, 0);
}
void cfgfile_dwrite_str (struct zfile *f, const TCHAR *option, const TCHAR *value)
{
	cfg_dowrite (f, option, value, 1, 0);
}
void cfgfile_dwrite_str (struct zfile *f, const TCHAR *option, const TCHAR *optionext, const TCHAR *value)
{
	cfg_dowrite (f, option, optionext, value, 1, 0);
}

void cfgfile_target_write_bool (struct zfile *f, const TCHAR *option, bool b)
{
	cfg_dowrite (f, option, b ? _T("true") : _T("false"), 0, 1);
}
void cfgfile_target_dwrite_bool (struct zfile *f, const TCHAR *option, bool b)
{
	cfg_dowrite (f, option, b ? _T("true") : _T("false"), 1, 1);
}
void cfgfile_target_write_str (struct zfile *f, const TCHAR *option, const TCHAR *value)
{
	cfg_dowrite (f, option, value, 0, 1);
}
void cfgfile_target_dwrite_str (struct zfile *f, const TCHAR *option, const TCHAR *value)
{
	cfg_dowrite (f, option, value, 1, 1);
}

void cfgfile_write_ext (struct zfile *f, const TCHAR *option, const TCHAR *optionext, const TCHAR *format,...)
{
	va_list parms;
	TCHAR tmp[CONFIG_BLEN], tmp2[CONFIG_BLEN];

	if (optionext) {
		_tcscpy (tmp2, option);
		_tcscat (tmp2, optionext);
	}
	va_start (parms, format);
	_vsntprintf (tmp, CONFIG_BLEN, format, parms);
	cfg_dowrite (f, optionext ? tmp2 : option, tmp, 0, 0);
	va_end (parms);
}
void cfgfile_write (struct zfile *f, const TCHAR *option, const TCHAR *format,...)
{
	va_list parms;
	TCHAR tmp[CONFIG_BLEN];

	va_start (parms, format);
	_vsntprintf (tmp, CONFIG_BLEN, format, parms);
	cfg_dowrite (f, option, tmp, 0, 0);
	va_end (parms);
}
void cfgfile_dwrite_ext (struct zfile *f, const TCHAR *option, const TCHAR *optionext, const TCHAR *format,...)
{
	va_list parms;
	TCHAR tmp[CONFIG_BLEN], tmp2[CONFIG_BLEN];

	if (optionext) {
		_tcscpy (tmp2, option);
		_tcscat (tmp2, optionext);
	}
	va_start (parms, format);
	_vsntprintf (tmp, CONFIG_BLEN, format, parms);
	cfg_dowrite (f, optionext ? tmp2 : option, tmp, 1, 0);
	va_end (parms);
}
void cfgfile_dwrite (struct zfile *f, const TCHAR *option, const TCHAR *format,...)
{
	va_list parms;
	TCHAR tmp[CONFIG_BLEN];

	va_start (parms, format);
	_vsntprintf (tmp, CONFIG_BLEN, format, parms);
	cfg_dowrite (f, option, tmp, 1, 0);
	va_end (parms);
}
void cfgfile_target_write (struct zfile *f, const TCHAR *option, const TCHAR *format,...)
{
	va_list parms;
	TCHAR tmp[CONFIG_BLEN];

	va_start (parms, format);
	_vsntprintf (tmp, CONFIG_BLEN, format, parms);
	cfg_dowrite (f, option, tmp, 0, 1);
	va_end (parms);
}
void cfgfile_target_dwrite (struct zfile *f, const TCHAR *option, const TCHAR *format,...)
{
	va_list parms;
	TCHAR tmp[CONFIG_BLEN];

	va_start (parms, format);
	_vsntprintf (tmp, CONFIG_BLEN, format, parms);
	cfg_dowrite (f, option, tmp, 1, 1);
	va_end (parms);
}

static void cfgfile_write_rom (struct zfile *f, struct multipath *mp, const TCHAR *romfile, const TCHAR *name)
{
	TCHAR *str = cfgfile_subst_path (mp->path[0], UNEXPANDED, romfile);
	str = cfgfile_put_multipath (mp, str);
	cfgfile_write_str (f, name, str);
	struct zfile *zf = zfile_fopen (str, _T("rb"), ZFD_ALL);
	if (zf) {
		struct romdata *rd = getromdatabyzfile (zf);
		if (rd) {
			TCHAR name2[MAX_DPATH], str2[MAX_DPATH];
			_tcscpy (name2, name);
			_tcscat (name2, _T("_id"));
			_stprintf (str2, _T("%08X,%s"), rd->crc32, rd->name);
			cfgfile_write_str (f, name2, str2);
		}
		zfile_fclose (zf);
	}
	xfree (str);

}

static void cfgfile_write_path (struct zfile *f, struct multipath *mp, const TCHAR *option, const TCHAR *value)
{
	TCHAR *s = cfgfile_put_multipath (mp, value);
	cfgfile_write_str (f, option, s);
	xfree (s);
}
static void cfgfile_dwrite_path (struct zfile *f, struct multipath *mp, const TCHAR *option, const TCHAR *value)
{
	TCHAR *s = cfgfile_put_multipath (mp, value);
	cfgfile_dwrite_str (f, option, s);
	xfree (s);
}

static void write_filesys_config (struct uae_prefs *p, struct zfile *f)
{
	int i;
	TCHAR tmp[MAX_DPATH], tmp2[MAX_DPATH], tmp3[MAX_DPATH], hdcs[MAX_DPATH];

	for (i = 0; i < p->mountitems; i++) {
		struct uaedev_config_data *uci = &p->mountconfig[i];
		struct uaedev_config_info *ci = &uci->ci;
		TCHAR *str1, *str2, *str1b, *str2b;
		int bp = ci->bootpri;

		str2 = _T("");
		if (ci->rootdir[0] == ':') {
			TCHAR *ptr;
			// separate harddrive names
			str1 = my_strdup (ci->rootdir);
			ptr = _tcschr (str1 + 1, ':');
			if (ptr) {
				*ptr++ = 0;
				str2 = ptr;
				ptr = _tcschr (str2, ',');
				if (ptr)
					*ptr = 0;
			}
		} else {
			str1 = cfgfile_put_multipath (&p->path_hardfile, ci->rootdir);
		}
		int ct = ci->controller_type;
		if (ct >= HD_CONTROLLER_TYPE_SCSI_EXPANSION_FIRST && ct <= HD_CONTROLLER_TYPE_SCSI_LAST) {
			_stprintf(hdcs, _T("scsi%d_%s"), ci->controller_unit, expansionroms[ct - HD_CONTROLLER_TYPE_SCSI_EXPANSION_FIRST].name);
		} else if (ct >= HD_CONTROLLER_TYPE_IDE_EXPANSION_FIRST && ct <= HD_CONTROLLER_TYPE_IDE_LAST) {
			_stprintf(hdcs, _T("ide%d_%s"), ci->controller_unit, expansionroms[ct - HD_CONTROLLER_TYPE_IDE_EXPANSION_FIRST].name);
		} else if (ct >= HD_CONTROLLER_TYPE_SCSI_FIRST && ct < HD_CONTROLLER_TYPE_SCSI_EXPANSION_FIRST) {
			_stprintf(hdcs, hdcontrollers[ct - HD_CONTROLLER_EXPANSION_MAX], ci->controller_unit);
		} else if (ct >= HD_CONTROLLER_TYPE_PCMCIA_SRAM && ct <= HD_CONTROLLER_TYPE_PCMCIA_IDE) {
			_stprintf(hdcs, hdcontrollers[ct - 2 * HD_CONTROLLER_EXPANSION_MAX], ci->controller_unit);
		} else {
			_stprintf(hdcs, hdcontrollers[ct], ci->controller_unit);
		}
		if (ci->controller_type_unit > 0)
			_stprintf(hdcs + _tcslen(hdcs), _T("-%d"), ci->controller_type_unit + 1);

		str1b = cfgfile_escape (str1, _T(":,"), true);
		str2b = cfgfile_escape (str2, _T(":,"), true);
		if (ci->type == UAEDEV_DIR) {
			_stprintf (tmp, _T("%s,%s:%s:%s,%d"), ci->readonly ? _T("ro") : _T("rw"),
				ci->devname ? ci->devname : _T(""), ci->volname, str1, bp);
			cfgfile_write_str (f, _T("filesystem2"), tmp);
			_tcscpy (tmp3, tmp);
#if 0
			_stprintf (tmp2, _T("filesystem=%s,%s:%s"), uci->readonly ? _T("ro") : _T("rw"),
				uci->volname, str);
			zfile_fputs (f, tmp2);
#endif
		} else if (ci->type == UAEDEV_HDF || ci->type == UAEDEV_CD || ci->type == UAEDEV_TAPE) {
			_stprintf (tmp, _T("%s,%s:%s,%d,%d,%d,%d,%d,%s,%s"),
				ci->readonly ? _T("ro") : _T("rw"),
				ci->devname ? ci->devname : _T(""), str1,
				ci->sectors, ci->surfaces, ci->reserved, ci->blocksize,
				bp, ci->filesys ? ci->filesys : _T(""), hdcs);
			_stprintf (tmp3, _T("%s,%s:%s%s%s,%d,%d,%d,%d,%d,%s,%s"),
				ci->readonly ? _T("ro") : _T("rw"),
				ci->devname ? ci->devname : _T(""), str1b, str2b[0] ? _T(":") : _T(""), str2b,
				ci->sectors, ci->surfaces, ci->reserved, ci->blocksize,
				bp, ci->filesys ? ci->filesys : _T(""), hdcs);
			if (ci->highcyl || ci->physical_geometry) {
				TCHAR *s = tmp + _tcslen (tmp);
				TCHAR *s2 = s;
				_stprintf (s2, _T(",%d"), ci->highcyl);
				if (ci->physical_geometry && ci->pheads && ci->psecs) {
					TCHAR *s = tmp + _tcslen (tmp);
					_stprintf (s, _T(",%d/%d/%d"), ci->pcyls, ci->pheads, ci->psecs);
				}
				_tcscat (tmp3, s2);
			}
			if (ci->controller_media_type) {
				_tcscat(tmp, _T(",CF"));
				_tcscat(tmp3, _T(",CF"));
			}
			TCHAR *extras = NULL;
			if (ct >= HD_CONTROLLER_TYPE_SCSI_FIRST && ct <= HD_CONTROLLER_TYPE_SCSI_LAST) {
				if (ci->unit_feature_level == HD_LEVEL_SCSI_1){
					extras = _T("SCSI1");
				}	else if (ci->unit_feature_level == HD_LEVEL_SASI) {
					extras = _T("SASI");
				} else if (ci->unit_feature_level == HD_LEVEL_SASI_ENHANCED) {
					extras = _T("SASIE");
				} else if (ci->unit_feature_level == HD_LEVEL_OMTI) {
					extras = _T("SASI_CHS");
				}
			} else if (ct >= HD_CONTROLLER_TYPE_IDE_FIRST && ct <= HD_CONTROLLER_TYPE_IDE_LAST) {
				if (ci->unit_feature_level == HD_LEVEL_ATA_1) {
					extras = _T("ATA1");
				} else if (ci->unit_feature_level == HD_LEVEL_ATA_2S) {
					extras = _T("ATA2+S");
				}
			}
			if (extras) {
				_tcscat(tmp, _T(","));
				_tcscat(tmp3, _T(","));
				_tcscat(tmp, extras);
				_tcscat(tmp3, extras);
			}
			if (ci->type == UAEDEV_HDF)
				cfgfile_write_str (f, _T("hardfile2"), tmp);
#if 0
			_stprintf (tmp2, _T("hardfile=%s,%d,%d,%d,%d,%s"),
				uci->readonly ? "ro" : "rw", uci->sectors,
				uci->surfaces, uci->reserved, uci->blocksize, str);
			zfile_fputs (f, tmp2);
#endif
		}
		_stprintf (tmp2, _T("uaehf%d"), i);
		if (ci->type == UAEDEV_CD) {
			cfgfile_write (f, tmp2, _T("cd%d,%s"), ci->device_emu_unit, tmp);
		} else if (ci->type == UAEDEV_TAPE) {
			cfgfile_write (f, tmp2, _T("tape%d,%s"), ci->device_emu_unit, tmp);
		} else {
			cfgfile_write (f, tmp2, _T("%s,%s"), ci->type == UAEDEV_HDF ? _T("hdf") : _T("dir"), tmp3);
		}
		if (ci->type == UAEDEV_DIR) {
			bool add_extra = false;
			if (ci->inject_icons) {
				add_extra = true;
			}
			if (add_extra) {
				_stprintf(tmp2, _T("%s,inject_icons=%s"), ci->devname, ci->inject_icons ? _T("true") : _T("false"));
				cfgfile_write(f, _T("filesystem_extra"), tmp2);
			}
		}
		xfree (str1b);
		xfree (str2b);
		xfree (str1);
		
	}
}

static void write_compatibility_cpu (struct zfile *f, struct uae_prefs *p)
{
	TCHAR tmp[100];
	int model;

	model = p->cpu_model;
	if (model == 68030)
		model = 68020;
	if (model == 68060)
		model = 68040;
	if (p->address_space_24 && model == 68020)
		_tcscpy (tmp, _T("68ec020"));
	else
		_stprintf (tmp, _T("%d"), model);
	if (model == 68020 && (p->fpu_model == 68881 || p->fpu_model == 68882))
		_tcscat (tmp, _T("/68881"));
	cfgfile_write (f, _T("cpu_type"), tmp);
}

static void write_leds (struct zfile *f, const TCHAR *name, int mask)
{
	TCHAR tmp[MAX_DPATH];
	tmp[0] = 0;
	for (int i = 0; leds[i]; i++) {
		bool got = false;
		for (int j = 0; leds[j]; j++) {
			if (leds_order[j] == i) {
				if (mask & (1 << j)) {
					if (got)
						_tcscat (tmp, _T(":"));
					_tcscat (tmp, leds[j]);
					got = true;
				}
			}
		}
		if (leds[i + 1] && got)
			_tcscat (tmp, _T(","));
	}
	while (tmp[0] && tmp[_tcslen (tmp) - 1] == ',')
		tmp[_tcslen (tmp) - 1] = 0;
	cfgfile_dwrite_str (f, name, tmp);
}

static void write_resolution (struct zfile *f, const TCHAR *ws, const TCHAR *hs, struct wh *wh)
{
	if (wh->width <= 0 || wh->height <= 0 || wh->special == WH_NATIVE) {
		cfgfile_write_str (f, ws, _T("native"));
		cfgfile_write_str (f, hs, _T("native"));
	} else {
		cfgfile_write (f, ws, _T("%d"), wh->width);
		cfgfile_write (f, hs, _T("%d"), wh->height);
	}
}

static void cfgfile_write_board_rom(struct zfile *f, struct multipath *mp, struct boardromconfig *br)
{
	TCHAR buf[256];
	TCHAR name[256];
	const struct expansionromtype *ert;
	
	if (br->device_type == 0)
		return;
	ert = get_device_expansion_rom(br->device_type);
	if (!ert)
		return;
	for (int i = 0; i < MAX_BOARD_ROMS; i++) {
		if (br->device_num == 0)
			_tcscpy(name, ert->name);
		else
			_stprintf(name, _T("%s-%d"), ert->name, br->device_num + 1);
		if (i == 0 || _tcslen(br->roms[i].romfile)) {
			_stprintf(buf, _T("%s%s_rom_file"), name, i ? _T("_ext") : _T(""));
			cfgfile_write_rom (f, mp, br->roms[i].romfile, buf);
			if (br->roms[i].romident[0]) {
				_stprintf(buf, _T("%s%s_rom"), name, i ? _T("_ext") : _T(""));
				cfgfile_dwrite_str (f, buf, br->roms[i].romident);
			}
			if (br->roms[i].autoboot_disabled || ert->subtypes || ert->settings || ert->id_jumper) {
				TCHAR buf2[256], *p;
				buf2[0] = 0;
				p = buf2;
				_stprintf(buf, _T("%s%s_rom_options"), name, i ? _T("_ext") : _T(""));
				if (ert->subtypes) {
					const struct expansionsubromtype *srt = ert->subtypes;
					int k = br->roms[i].subtype;
					while (k && srt[1].name) {
						srt++;
						k--;
					}
					_stprintf(p, _T("subtype=%s"), srt->configname);
				}
				if (br->roms[i].autoboot_disabled) {
					if (buf2[0])
						_tcscat(buf2, _T(","));
					_tcscat(buf2, _T("autoboot_disabled=true"));
				}
				if (ert->id_jumper) {
					TCHAR tmp[256];
					_stprintf(tmp, _T("id=%d"), br->roms[i].device_id);
					if (buf2[0])
						_tcscat(buf2, _T(","));
					_tcscat(buf2, tmp);
				}
				if (br->roms[i].device_settings && ert->settings) {
					for (int j = 0; ert->settings[j].name; j++) {
						if (br->roms[i].device_settings & (1 << j)) {
							if (buf2[0])
								_tcscat(buf2, _T(","));
							_tcscat(buf2, ert->settings[j].configname);
						}
					}
				}
				if (buf2[0])
					cfgfile_dwrite_str (f, buf, buf2);
			}

			if (br->roms[i].board_ram_size) {
				_stprintf(buf, _T("%s%s_mem_size"), name, i ? _T("_ext") : _T(""));
				cfgfile_write(f, buf, _T("%d"), br->roms[i].board_ram_size / 0x40000);
			}
		}
	}
}

void cfgfile_save_options (struct zfile *f, struct uae_prefs *p, int type)
{
	struct strlist *sl;
	TCHAR tmp[MAX_DPATH];
	int i;

	cfgfile_write_str (f, _T("config_description"), p->description);
	cfgfile_write_bool (f, _T("config_hardware"), type & CONFIG_TYPE_HARDWARE);
	cfgfile_write_bool (f, _T("config_host"), !!(type & CONFIG_TYPE_HOST));
	if (p->info[0])
		cfgfile_write (f, _T("config_info"), p->info);
	cfgfile_write (f, _T("config_version"), _T("%d.%d.%d"), UAEMAJOR, UAEMINOR, UAESUBREV);
	cfgfile_write_str (f, _T("config_hardware_path"), p->config_hardware_path);
	cfgfile_write_str (f, _T("config_host_path"), p->config_host_path);
	if (p->config_window_title[0])
		cfgfile_write_str (f, _T("config_window_title"), p->config_window_title);

	for (sl = p->all_lines; sl; sl = sl->next) {
		if (sl->unknown) {
			if (sl->option)
				cfgfile_write_str (f, sl->option, sl->value);
		}
	}

	for (i = 0; i < MAX_PATHS; i++) {
		if (p->path_rom.path[i][0]) {
			_stprintf (tmp, _T("%s.rom_path"), TARGET_NAME);
			cfgfile_write_str (f, tmp, p->path_rom.path[i]);
		}
	}
	for (i = 0; i < MAX_PATHS; i++) {
		if (p->path_floppy.path[i][0]) {
			_stprintf (tmp, _T("%s.floppy_path"), TARGET_NAME);
			cfgfile_write_str (f, tmp, p->path_floppy.path[i]);
		}
	}
	for (i = 0; i < MAX_PATHS; i++) {
		if (p->path_hardfile.path[i][0]) {
			_stprintf (tmp, _T("%s.hardfile_path"), TARGET_NAME);
			cfgfile_write_str (f, tmp, p->path_hardfile.path[i]);
		}
	}
	for (i = 0; i < MAX_PATHS; i++) {
		if (p->path_cd.path[i][0]) {
			_stprintf (tmp, _T("%s.cd_path"), TARGET_NAME);
			cfgfile_write_str (f, tmp, p->path_cd.path[i]);
		}
	}

	cfg_write (_T("; host-specific"), f);

	target_save_options (f, p);

	cfg_write (_T("; common"), f);

	cfgfile_write_str (f, _T("use_gui"), guimode1[p->start_gui]);
	cfgfile_write_bool (f, _T("use_debugger"), p->start_debugger);

	cfgfile_write_rom (f, &p->path_rom, p->romfile, _T("kickstart_rom_file"));
	cfgfile_write_rom (f, &p->path_rom, p->romextfile, _T("kickstart_ext_rom_file"));
	if (p->romextfile2addr) {
		cfgfile_write (f, _T("kickstart_ext_rom_file2_address"), _T("%x"), p->romextfile2addr);
		cfgfile_write_rom (f, &p->path_rom, p->romextfile2, _T("kickstart_ext_rom_file2"));
	}
	if (p->romident[0])
		cfgfile_dwrite_str (f, _T("kickstart_rom"), p->romident);
	if (p->romextident[0])
		cfgfile_write_str (f, _T("kickstart_ext_rom="), p->romextident);

	for (int i = 0; i < MAX_EXPANSION_BOARDS; i++) {
		cfgfile_write_board_rom(f, &p->path_rom, &p->expansionboard[i]);
	}

	cfgfile_write_path (f, &p->path_rom, _T("flash_file"), p->flashfile);
	cfgfile_write_path (f, &p->path_rom, _T("cart_file"), p->cartfile);
	cfgfile_write_path (f, &p->path_rom, _T("rtc_file"), p->rtcfile);
	if (p->cartident[0])
		cfgfile_write_str (f, _T("cart"), p->cartident);
	cfgfile_dwrite_path (f, &p->path_rom, _T("picassoiv_rom_file"), p->picassoivromfile);

	cfgfile_write_bool (f, _T("kickshifter"), p->kickshifter);
	cfgfile_write_bool (f, _T("ks_write_enabled"), p->rom_readwrite);

	cfgfile_write (f, _T("floppy_volume"), _T("%d"), p->dfxclickvolume_disk[0]);
	p->nr_floppies = 4;
	for (i = 0; i < 4; i++) {
		_stprintf (tmp, _T("floppy%d"), i);
		cfgfile_write_path (f, &p->path_floppy, tmp, p->floppyslots[i].df);
		_stprintf (tmp, _T("floppy%dwp"), i);
		cfgfile_dwrite_bool (f, tmp, p->floppyslots[i].forcedwriteprotect);
		_stprintf (tmp, _T("floppy%dtype"), i);
		cfgfile_dwrite (f, tmp, _T("%d"), p->floppyslots[i].dfxtype);
		_stprintf (tmp, _T("floppy%dsound"), i);
		cfgfile_dwrite (f, tmp, _T("%d"), p->floppyslots[i].dfxclick);
		if (p->floppyslots[i].dfxclick < 0 && p->floppyslots[i].dfxclickexternal[0]) {
			_stprintf (tmp, _T("floppy%dsoundext"), i);
			cfgfile_dwrite (f, tmp, p->floppyslots[i].dfxclickexternal);
		}
		if (p->floppyslots[i].dfxclick) {
			_stprintf (tmp, _T("floppy%dsoundvolume_disk"), i);
			cfgfile_write (f, tmp, _T("%d"), p->dfxclickvolume_disk[i]);
			_stprintf (tmp, _T("floppy%dsoundvolume_empty"), i);
			cfgfile_write (f, tmp, _T("%d"), p->dfxclickvolume_empty[i]);
		}
		if (p->floppyslots[i].dfxtype < 0 && p->nr_floppies > i)
			p->nr_floppies = i;
	}
	for (i = 0; i < MAX_SPARE_DRIVES; i++) {
		if (p->dfxlist[i][0]) {
			_stprintf (tmp, _T("diskimage%d"), i);
			cfgfile_dwrite_path (f, &p->path_floppy, tmp, p->dfxlist[i]);
		}
	}

	for (i = 0; i < MAX_TOTAL_SCSI_DEVICES; i++) {
		if (p->cdslots[i].name[0] || p->cdslots[i].inuse) {
			TCHAR tmp2[MAX_DPATH];
			_stprintf (tmp, _T("cdimage%d"), i);
			TCHAR *s = cfgfile_put_multipath (&p->path_cd, p->cdslots[i].name);
			_tcscpy (tmp2, s);
			xfree (s);
			if (p->cdslots[i].type != SCSI_UNIT_DEFAULT || _tcschr (p->cdslots[i].name, ',') || p->cdslots[i].delayed) {
				_tcscat (tmp2, _T(","));
				if (p->cdslots[i].delayed) {
					_tcscat (tmp2, _T("delay"));
					_tcscat (tmp2, _T(":"));
				}
				if (p->cdslots[i].type != SCSI_UNIT_DEFAULT) {
					_tcscat (tmp2, cdmodes[p->cdslots[i].type + 1]);
				}
			}
			cfgfile_write_str (f, tmp, tmp2);
		}
	}

	for (i = 0; i < MAX_LUA_STATES; i++) {
		if (p->luafiles[i][0]) {
			cfgfile_write_str (f, _T("lua"), p->luafiles[i]);
		}
	}

	if (p->statefile[0])
		cfgfile_write_str (f, _T("statefile"), p->statefile);
	if (p->quitstatefile[0])
		cfgfile_write_str (f, _T("statefile_quit"), p->quitstatefile);

	cfgfile_write (f, _T("nr_floppies"), _T("%d"), p->nr_floppies);
	cfgfile_dwrite_bool (f, _T("floppy_write_protect"), p->floppy_read_only);
	cfgfile_write (f, _T("floppy_speed"), _T("%d"), p->floppy_speed);
	cfgfile_dwrite (f, _T("floppy_channel_mask"), _T("0x%x"), p->dfxclickchannelmask);
	cfgfile_write (f, _T("cd_speed"), _T("%d"), p->cd_speed);
	cfgfile_write_bool (f, _T("parallel_on_demand"), p->parallel_demand);
	cfgfile_write_bool (f, _T("serial_on_demand"), p->serial_demand);
	cfgfile_write_bool (f, _T("serial_hardware_ctsrts"), p->serial_hwctsrts);
	cfgfile_write_bool (f, _T("serial_direct"), p->serial_direct);
	cfgfile_dwrite (f, _T("serial_stopbits"), _T("%d"), p->serial_stopbits);
	cfgfile_dwrite_str(f, _T("serial_translate"), serialcrlf[p->serial_crlf]);
	cfgfile_write_str (f, _T("scsi"), scsimode[p->scsi]);
	cfgfile_write_bool (f, _T("uaeserial"), p->uaeserial);
	cfgfile_write_bool (f, _T("sana2"), p->sana2);

	cfgfile_write_str (f, _T("sound_output"), soundmode1[p->produce_sound]);
	cfgfile_write_str (f, _T("sound_channels"), stereomode[p->sound_stereo]);
	cfgfile_write (f, _T("sound_stereo_separation"), _T("%d"), p->sound_stereo_separation);
	cfgfile_write (f, _T("sound_stereo_mixing_delay"), _T("%d"), p->sound_mixed_stereo_delay >= 0 ? p->sound_mixed_stereo_delay : 0);
	cfgfile_write (f, _T("sound_max_buff"), _T("%d"), p->sound_maxbsiz);
	cfgfile_write (f, _T("sound_frequency"), _T("%d"), p->sound_freq);
	cfgfile_write_str (f, _T("sound_interpol"), interpolmode[p->sound_interpol]);
	cfgfile_write_str (f, _T("sound_filter"), soundfiltermode1[p->sound_filter]);
	cfgfile_write_str (f, _T("sound_filter_type"), soundfiltermode2[p->sound_filter_type]);
	cfgfile_write (f, _T("sound_volume"), _T("%d"), p->sound_volume_master);
	cfgfile_write (f, _T("sound_volume_paula"), _T("%d"), p->sound_volume_paula);
	if (p->sound_volume_cd >= 0)
		cfgfile_write (f, _T("sound_volume_cd"), _T("%d"), p->sound_volume_cd);
	if (p->sound_volume_board >= 0)
		cfgfile_write (f, _T("sound_volume_ahi"), _T("%d"), p->sound_volume_board);
	cfgfile_write_bool (f, _T("sound_auto"), p->sound_auto);
	cfgfile_write_bool (f, _T("sound_cdaudio"), p->sound_cdaudio);
	cfgfile_write_bool (f, _T("sound_stereo_swap_paula"), p->sound_stereo_swap_paula);
	cfgfile_write_bool (f, _T("sound_stereo_swap_ahi"), p->sound_stereo_swap_ahi);
	cfgfile_dwrite (f, _T("sampler_frequency"), _T("%d"), p->sampler_freq);
	cfgfile_dwrite (f, _T("sampler_buffer"), _T("%d"), p->sampler_buffer);
	cfgfile_dwrite_bool (f, _T("sampler_stereo"), p->sampler_stereo);

	cfgfile_write_str (f, _T("comp_trustbyte"), compmode[p->comptrustbyte]);
	cfgfile_write_str (f, _T("comp_trustword"), compmode[p->comptrustword]);
	cfgfile_write_str (f, _T("comp_trustlong"), compmode[p->comptrustlong]);
	cfgfile_write_str (f, _T("comp_trustnaddr"), compmode[p->comptrustnaddr]);
	cfgfile_write_bool (f, _T("comp_nf"), p->compnf);
	cfgfile_write_bool (f, _T("comp_constjump"), p->comp_constjump);
	cfgfile_write_bool (f, _T("comp_oldsegv"), p->comp_oldsegv);
	cfgfile_write_str (f, _T("comp_flushmode"), flushmode[p->comp_hardflush]);
	cfgfile_write_bool (f, _T("compfpu"), p->compfpu);
	cfgfile_write_bool (f, _T("comp_midopt"), p->comp_midopt);
	cfgfile_write_bool (f, _T("comp_lowopt"), p->comp_lowopt);
	cfgfile_write_bool (f, _T("avoid_cmov"), p->avoid_cmov);
	cfgfile_write (f, _T("cachesize"), _T("%d"), p->cachesize);

	for (i = 0; i < MAX_JPORTS; i++) {
		struct jport *jp = &p->jports[i];
		int v = jp->id;
		TCHAR tmp1[MAX_DPATH], tmp2[MAX_DPATH];
		if (v == JPORT_CUSTOM) {
			_tcscpy (tmp2, _T("custom"));
		} else if (v == JPORT_NONE) {
			_tcscpy (tmp2, _T("none"));
		} else if (v < JSEM_JOYS) {
			_stprintf (tmp2, _T("kbd%d"), v + 1);
		} else if (v < JSEM_MICE) {
			_stprintf (tmp2, _T("joy%d"), v - JSEM_JOYS);
		} else {
			_tcscpy (tmp2, _T("mouse"));
			if (v - JSEM_MICE > 0)
				_stprintf (tmp2, _T("mouse%d"), v - JSEM_MICE);
		}
		if (i < 2 || jp->id >= 0) {
			_stprintf (tmp1, _T("joyport%d"), i);
			cfgfile_write (f, tmp1, tmp2);
			_stprintf (tmp1, _T("joyport%dautofire"), i);
			cfgfile_write (f, tmp1, joyaf[jp->autofire]);
			if (i < 2 && jp->mode > 0) {
				_stprintf (tmp1, _T("joyport%dmode"), i);
				cfgfile_write (f, tmp1, joyportmodes[jp->mode]);
			}
			if (jp->name[0]) {
				_stprintf (tmp1, _T("joyportfriendlyname%d"), i);
				cfgfile_write (f, tmp1, jp->name);
			}
			if (jp->configname[0]) {
				_stprintf (tmp1, _T("joyportname%d"), i);
				cfgfile_write (f, tmp1, jp->configname);
			}
			if (jp->nokeyboardoverride) {
				_stprintf (tmp1, _T("joyport%dkeyboardoverride"), i);
				cfgfile_write_bool (f, tmp1, !jp->nokeyboardoverride);
			}
		}
	}
	if (p->dongle) {
		if (p->dongle + 1 >= sizeof (dongles) / sizeof (TCHAR*))
			cfgfile_write (f, _T("dongle"), _T("%d"), p->dongle);
		else
			cfgfile_write_str (f, _T("dongle"), dongles[p->dongle]);
	}

	cfgfile_write_bool (f, _T("bsdsocket_emu"), p->socket_emu);
	if (p->a2065name[0])
		cfgfile_write_str (f, _T("a2065"), p->a2065name);
	if (p->ne2000pciname[0])
		cfgfile_write_str(f, _T("ne2000_pci"), p->ne2000pciname);

#ifdef WITH_SLIRP
	tmp[0] = 0;
	for (i = 0; i < MAX_SLIRP_REDIRS; i++) {
		struct slirp_redir *sr = &p->slirp_redirs[i];
		if (sr->proto && !sr->srcport) {
			TCHAR *p = tmp + _tcslen (tmp);
			if (p > tmp)
				*p++ = ',';
			_stprintf (p, _T("%d"), sr->dstport);
		}
	}
	if (tmp[0])
		cfgfile_write_str (f, _T("slirp_ports"), tmp);
	for (i = 0; i < MAX_SLIRP_REDIRS; i++) {
		struct slirp_redir *sr = &p->slirp_redirs[i];
		if (sr->proto && sr->srcport) {
			uae_u32 v = htonl (sr->addr);
			if (v) {
				_stprintf (tmp, _T("%s:%d:%d:%d.%d.%d.%d"),
					sr->proto == 1 ? _T("tcp") : _T("udp"),
					sr->dstport, sr->srcport,
					(v >> 24) & 0xff, (v >> 16) & 0xff, (v >> 8) & 0xff, v & 0xff);
			} else {
				_stprintf (tmp, _T("%s:%d:%d"),
					sr->proto == 1 ? _T("tcp") : _T("udp"),
					sr->dstport, sr->srcport);
			}
			cfgfile_write_str (f, _T("slirp_redir"), tmp);
		}
	}
#endif

	cfgfile_write_bool (f, _T("synchronize_clock"), p->tod_hack);
	cfgfile_write (f, _T("maprom"), _T("0x%x"), p->maprom);
	cfgfile_dwrite_str (f, _T("boot_rom_uae"), uaebootrom[p->boot_rom]);
	cfgfile_dwrite_str (f, _T("parallel_matrix_emulation"), epsonprinter[p->parallel_matrix_emulation]);
	cfgfile_write_bool (f, _T("parallel_postscript_emulation"), p->parallel_postscript_emulation);
	cfgfile_write_bool (f, _T("parallel_postscript_detection"), p->parallel_postscript_detection);
	cfgfile_write_str (f, _T("ghostscript_parameters"), p->ghostscript_parameters);
	cfgfile_write (f, _T("parallel_autoflush"), _T("%d"), p->parallel_autoflush_time);
	cfgfile_dwrite (f, _T("uae_hide"), _T("%d"), p->uae_hide);
	cfgfile_dwrite_bool (f, _T("uae_hide_autoconfig"), p->uae_hide_autoconfig);
	cfgfile_dwrite_bool (f, _T("magic_mouse"), p->input_magic_mouse);
	cfgfile_dwrite_str (f, _T("magic_mousecursor"), magiccursors[p->input_magic_mouse_cursor]);
	cfgfile_dwrite_str (f, _T("absolute_mouse"), abspointers[p->input_tablet]);
	cfgfile_dwrite_bool (f, _T("tablet_library"), p->tablet_library);
	cfgfile_dwrite_bool (f, _T("clipboard_sharing"), p->clipboard_sharing);
	cfgfile_dwrite_bool (f, _T("native_code"), p->native_code);

	cfgfile_write (f, _T("gfx_display"), _T("%d"), p->gfx_apmode[APMODE_NATIVE].gfx_display);
	cfgfile_write_str (f, _T("gfx_display_friendlyname"), target_get_display_name (p->gfx_apmode[APMODE_NATIVE].gfx_display, true));
	cfgfile_write_str (f, _T("gfx_display_name"), target_get_display_name (p->gfx_apmode[APMODE_NATIVE].gfx_display, false));
	cfgfile_write (f, _T("gfx_display_rtg"), _T("%d"), p->gfx_apmode[APMODE_RTG].gfx_display);
	cfgfile_write_str (f, _T("gfx_display_friendlyname_rtg"), target_get_display_name (p->gfx_apmode[APMODE_RTG].gfx_display, true));
	cfgfile_write_str (f, _T("gfx_display_name_rtg"), target_get_display_name (p->gfx_apmode[APMODE_RTG].gfx_display, false));

	cfgfile_write (f, _T("gfx_framerate"), _T("%d"), p->gfx_framerate);
	write_resolution (f, _T("gfx_width"), _T("gfx_height"), &p->gfx_size_win); /* compatibility with old versions */
	cfgfile_write (f, _T("gfx_top_windowed"), _T("%d"), p->gfx_size_win.x);
	cfgfile_write (f, _T("gfx_left_windowed"), _T("%d"), p->gfx_size_win.y);
	write_resolution (f, _T("gfx_width_windowed"), _T("gfx_height_windowed"), &p->gfx_size_win);
	write_resolution (f, _T("gfx_width_fullscreen"), _T("gfx_height_fullscreen"), &p->gfx_size_fs);
	cfgfile_write (f, _T("gfx_refreshrate"), _T("%d"), p->gfx_apmode[0].gfx_refreshrate);
	cfgfile_dwrite (f, _T("gfx_refreshrate_rtg"), _T("%d"), p->gfx_apmode[1].gfx_refreshrate);
	cfgfile_write (f, _T("gfx_autoresolution"), _T("%d"), p->gfx_autoresolution);
	cfgfile_dwrite (f, _T("gfx_autoresolution_delay"), _T("%d"), p->gfx_autoresolution_delay);
	cfgfile_dwrite (f, _T("gfx_autoresolution_min_vertical"), vertmode[p->gfx_autoresolution_minv + 1]);
	cfgfile_dwrite (f, _T("gfx_autoresolution_min_horizontal"), horizmode[p->gfx_autoresolution_minh + 1]);
	cfgfile_write_bool (f, _T("gfx_autoresolution_vga"), p->gfx_autoresolution_vga);

	cfgfile_write (f, _T("gfx_backbuffers"), _T("%d"), p->gfx_apmode[0].gfx_backbuffers);
	cfgfile_write (f, _T("gfx_backbuffers_rtg"), _T("%d"), p->gfx_apmode[1].gfx_backbuffers);
	if (p->gfx_apmode[APMODE_NATIVE].gfx_interlaced)
		cfgfile_write_bool (f, _T("gfx_interlace"), p->gfx_apmode[APMODE_NATIVE].gfx_interlaced);
	cfgfile_write_str (f, _T("gfx_vsync"), vsyncmodes[p->gfx_apmode[0].gfx_vsync]);
	cfgfile_write_str (f, _T("gfx_vsyncmode"), vsyncmodes2[p->gfx_apmode[0].gfx_vsyncmode]);
	cfgfile_write_str (f, _T("gfx_vsync_picasso"), vsyncmodes[p->gfx_apmode[1].gfx_vsync]);
	cfgfile_write_str (f, _T("gfx_vsyncmode_picasso"), vsyncmodes2[p->gfx_apmode[1].gfx_vsyncmode]);
	cfgfile_write_bool (f, _T("gfx_lores"), p->gfx_resolution == 0);
	cfgfile_write_str (f, _T("gfx_resolution"), lorestype1[p->gfx_resolution]);
	cfgfile_write_str (f, _T("gfx_lores_mode"), loresmode[p->gfx_lores_mode]);
	cfgfile_write_bool (f, _T("gfx_flickerfixer"), p->gfx_scandoubler);
	cfgfile_write_str (f, _T("gfx_linemode"), p->gfx_vresolution > 0 ? linemode[p->gfx_iscanlines * 4 + p->gfx_pscanlines + 1] : linemode[0]);
	cfgfile_write_str (f, _T("gfx_fullscreen_amiga"), fullmodes[p->gfx_apmode[0].gfx_fullscreen]);
	cfgfile_write_str (f, _T("gfx_fullscreen_picasso"), fullmodes[p->gfx_apmode[1].gfx_fullscreen]);
	cfgfile_write_str (f, _T("gfx_center_horizontal"), centermode1[p->gfx_xcenter]);
	cfgfile_write_str (f, _T("gfx_center_vertical"), centermode1[p->gfx_ycenter]);
	cfgfile_write_str (f, _T("gfx_colour_mode"), colormode1[p->color_mode]);
	cfgfile_write_bool (f, _T("gfx_blacker_than_black"), p->gfx_blackerthanblack);
	cfgfile_dwrite_bool (f, _T("gfx_black_frame_insertion"), p->lightboost_strobo);
	cfgfile_write_str (f, _T("gfx_api"), filterapi[p->gfx_api]);
	cfgfile_dwrite (f, _T("gfx_horizontal_tweak"), _T("%d"), p->gfx_extrawidth);

#ifdef GFXFILTER
	for (int j = 0; j < 2; j++) {
		struct gfx_filterdata *gf = &p->gf[j];
		const TCHAR *ext = j == 0 ? NULL : _T("_rtg");
		for (int i = 0; i <MAX_FILTERSHADERS; i++) {
			if (gf->gfx_filtershader[i][0])
				cfgfile_write_ext (f, _T("gfx_filter_pre"), ext, _T("D3D:%s"), gf->gfx_filtershader[i]);
			if (gf->gfx_filtermask[i][0])
				cfgfile_write_str (f, _T("gfx_filtermask_pre"), ext, gf->gfx_filtermask[i]);
		}
		for (int i = 0; i < MAX_FILTERSHADERS; i++) {
			if (gf->gfx_filtershader[i + MAX_FILTERSHADERS][0])
				cfgfile_write_ext (f, _T("gfx_filter_post"), ext, _T("D3D:%s"), gf->gfx_filtershader[i + MAX_FILTERSHADERS]);
			if (gf->gfx_filtermask[i + MAX_FILTERSHADERS][0])
				cfgfile_write_str (f, _T("gfx_filtermask_post"), ext, gf->gfx_filtermask[i + MAX_FILTERSHADERS]);
		}
		cfgfile_dwrite_str (f, _T("gfx_filter_mask"), ext, gf->gfx_filtermask[2 * MAX_FILTERSHADERS]);
		{
			bool d3dfound = false;
			if (gf->gfx_filtershader[2 * MAX_FILTERSHADERS][0] && p->gfx_api) {
				cfgfile_dwrite_ext (f, _T("gfx_filter"), ext, _T("D3D:%s"), gf->gfx_filtershader[2 * MAX_FILTERSHADERS]);
				d3dfound = true;
			}
			if (!d3dfound) {
				if (gf->gfx_filter > 0) {
					int i = 0;
					struct uae_filter *uf;
					while (uaefilters[i].name) {
						uf = &uaefilters[i];
						if (uf->type == gf->gfx_filter) {
							cfgfile_dwrite_str (f, _T("gfx_filter"), ext, uf->cfgname);
						}
						i++;
					}
				} else {
					cfgfile_dwrite_ext (f, _T("gfx_filter"), ext, _T("no"));
				}
			}
		}
		cfgfile_dwrite_str (f, _T("gfx_filter_mode"), ext, filtermode2[gf->gfx_filter_filtermode]);
		cfgfile_dwrite_ext (f, _T("gfx_filter_vert_zoomf"), ext, _T("%f"), gf->gfx_filter_vert_zoom);
		cfgfile_dwrite_ext (f, _T("gfx_filter_horiz_zoomf"), ext, _T("%f"), gf->gfx_filter_horiz_zoom);
		cfgfile_dwrite_ext (f, _T("gfx_filter_vert_zoom_multf"), ext, _T("%f"), gf->gfx_filter_vert_zoom_mult);
		cfgfile_dwrite_ext (f, _T("gfx_filter_horiz_zoom_multf"), ext, _T("%f"), gf->gfx_filter_horiz_zoom_mult);
		cfgfile_dwrite_ext (f, _T("gfx_filter_vert_offsetf"), ext, _T("%f"), gf->gfx_filter_vert_offset);
		cfgfile_dwrite_ext (f, _T("gfx_filter_horiz_offsetf"), ext, _T("%f"), gf->gfx_filter_horiz_offset);
		
		cfgfile_dwrite_ext (f, _T("gfx_filter_left_border"), ext, _T("%d"), gf->gfx_filter_left_border);
		cfgfile_dwrite_ext (f, _T("gfx_filter_right_border"), ext, _T("%d"), gf->gfx_filter_right_border);
		cfgfile_dwrite_ext (f, _T("gfx_filter_top_border"), ext, _T("%d"), gf->gfx_filter_top_border);
		cfgfile_dwrite_ext (f, _T("gfx_filter_bottom_border"), ext, _T("%d"), gf->gfx_filter_bottom_border);
		
		cfgfile_dwrite_ext (f, _T("gfx_filter_scanlines"), ext, _T("%d"), gf->gfx_filter_scanlines);
		cfgfile_dwrite_ext (f, _T("gfx_filter_scanlinelevel"), ext, _T("%d"), gf->gfx_filter_scanlinelevel);
		cfgfile_dwrite_ext (f, _T("gfx_filter_scanlineratio"), ext, _T("%d"), gf->gfx_filter_scanlineratio);
		
		cfgfile_dwrite_ext (f, _T("gfx_filter_luminance"), ext, _T("%d"), gf->gfx_filter_luminance);
		cfgfile_dwrite_ext (f, _T("gfx_filter_contrast"), ext, _T("%d"), gf->gfx_filter_contrast);
		cfgfile_dwrite_ext (f, _T("gfx_filter_saturation"), ext, _T("%d"), gf->gfx_filter_saturation);
		cfgfile_dwrite_ext (f, _T("gfx_filter_gamma"), ext, _T("%d"), gf->gfx_filter_gamma);
		cfgfile_dwrite_ext (f, _T("gfx_filter_gamma_r"), ext, _T("%d"), gf->gfx_filter_gamma_ch[0]);
		cfgfile_dwrite_ext (f, _T("gfx_filter_gamma_g"), ext, _T("%d"), gf->gfx_filter_gamma_ch[1]);
		cfgfile_dwrite_ext (f, _T("gfx_filter_gamma_b"), ext, _T("%d"), gf->gfx_filter_gamma_ch[2]);
		
		cfgfile_dwrite_ext (f, _T("gfx_filter_blur"), ext, _T("%d"), gf->gfx_filter_blur);
		cfgfile_dwrite_ext (f, _T("gfx_filter_noise"), ext, _T("%d"), gf->gfx_filter_noise);
		cfgfile_dwrite_bool (f, _T("gfx_filter_bilinear"), ext, gf->gfx_filter_bilinear != 0);
		
		cfgfile_dwrite_ext (f, _T("gfx_filter_keep_autoscale_aspect"), ext, _T("%d"), gf->gfx_filter_keep_autoscale_aspect);
		cfgfile_dwrite_str (f, _T("gfx_filter_keep_aspect"), ext, aspects[gf->gfx_filter_keep_aspect]);
		cfgfile_dwrite_str(f, _T("gfx_filter_autoscale"), ext, ext == NULL ? autoscale[gf->gfx_filter_autoscale] : autoscale_rtg[gf->gfx_filter_autoscale]);
		cfgfile_dwrite_str (f, _T("gfx_filter_autoscale_limit"), ext, autoscalelimit[gf->gfx_filter_integerscalelimit]);
		cfgfile_dwrite_ext (f, _T("gfx_filter_aspect_ratio"), ext, _T("%d:%d"),
			gf->gfx_filter_aspect >= 0 ? (gf->gfx_filter_aspect / ASPECTMULT) : -1,
			gf->gfx_filter_aspect >= 0 ? (gf->gfx_filter_aspect & (ASPECTMULT - 1)) : -1);
		if (gf->gfx_filteroverlay[0]) {
			cfgfile_dwrite_ext(f, _T("gfx_filter_overlay"), ext, _T("%s%s"),
				gf->gfx_filteroverlay, _tcschr (gf->gfx_filteroverlay, ',') ? _T(",") : _T(""));
		}
	}
	cfgfile_dwrite (f, _T("gfx_luminance"), _T("%d"), p->gfx_luminance);
	cfgfile_dwrite (f, _T("gfx_contrast"), _T("%d"), p->gfx_contrast);
	cfgfile_dwrite (f, _T("gfx_gamma"), _T("%d"), p->gfx_gamma);
	cfgfile_dwrite (f, _T("gfx_gamma_r"), _T("%d"), p->gfx_gamma_ch[0]);
	cfgfile_dwrite (f, _T("gfx_gamma_g"), _T("%d"), p->gfx_gamma_ch[1]);
	cfgfile_dwrite (f, _T("gfx_gamma_b"), _T("%d"), p->gfx_gamma_ch[2]);

	cfgfile_dwrite (f, _T("gfx_center_horizontal_position"), _T("%d"), p->gfx_xcenter_pos);
	cfgfile_dwrite (f, _T("gfx_center_vertical_position"), _T("%d"), p->gfx_ycenter_pos);
	cfgfile_dwrite (f, _T("gfx_center_horizontal_size"), _T("%d"), p->gfx_xcenter_size);
	cfgfile_dwrite (f, _T("gfx_center_vertical_size"), _T("%d"), p->gfx_ycenter_size);

	cfgfile_dwrite (f, _T("rtg_vert_zoom_multf"), _T("%.f"), p->rtg_vert_zoom_mult);
	cfgfile_dwrite (f, _T("rtg_horiz_zoom_multf"), _T("%.f"), p->rtg_horiz_zoom_mult);

#endif

	cfgfile_write_bool (f, _T("immediate_blits"), p->immediate_blits);
	cfgfile_dwrite_str (f, _T("waiting_blits"), waitblits[p->waiting_blits]);
	cfgfile_write_bool (f, _T("ntsc"), p->ntscmode);
	cfgfile_write_bool (f, _T("genlock"), p->genlock);
	cfgfile_dwrite_str(f, _T("genlockmode"), genlockmodes[p->genlock_image]);
	cfgfile_dwrite_str(f, _T("genlock_image"), p->genlock_image_file);
	cfgfile_dwrite(f, _T("genlock_mix"), _T("%d"), p->genlock_mix);
	cfgfile_dwrite_str(f, _T("monitoremu"), specialmonitors[p->monitoremu]);

	cfgfile_dwrite_bool (f, _T("show_leds"), !!(p->leds_on_screen & STATUSLINE_CHIPSET));
	cfgfile_dwrite_bool (f, _T("show_leds_rtg"), !!(p->leds_on_screen & STATUSLINE_RTG));
	write_leds(f, _T("show_leds_enabled"), p->leds_on_screen_mask[0]);
	write_leds(f, _T("show_leds_enabled_rtg"), p->leds_on_screen_mask[1]);

	if (p->osd_pos.y || p->osd_pos.x) {
		cfgfile_dwrite (f, _T("osd_position"), _T("%.1f%s:%.1f%s"),
			p->osd_pos.x >= 20000 ? (p->osd_pos.x - 30000) / 10.0 : (float)p->osd_pos.x, p->osd_pos.x >= 20000 ? _T("%") : _T(""),
			p->osd_pos.y >= 20000 ? (p->osd_pos.y - 30000) / 10.0 : (float)p->osd_pos.y, p->osd_pos.y >= 20000 ? _T("%") : _T(""));
	}
	cfgfile_dwrite (f, _T("keyboard_leds"), _T("numlock:%s,capslock:%s,scrolllock:%s"),
		kbleds[p->keyboard_leds[0]], kbleds[p->keyboard_leds[1]], kbleds[p->keyboard_leds[2]]);
	if (p->chipset_mask & CSMASK_AGA)
		cfgfile_write (f, _T("chipset"),_T("aga"));
	else if ((p->chipset_mask & CSMASK_ECS_AGNUS) && (p->chipset_mask & CSMASK_ECS_DENISE))
		cfgfile_write (f, _T("chipset"),_T("ecs"));
	else if (p->chipset_mask & CSMASK_ECS_AGNUS)
		cfgfile_write (f, _T("chipset"),_T("ecs_agnus"));
	else if (p->chipset_mask & CSMASK_ECS_DENISE)
		cfgfile_write (f, _T("chipset"),_T("ecs_denise"));
	else
		cfgfile_write (f, _T("chipset"), _T("ocs"));
	if (p->chipset_refreshrate > 0)
		cfgfile_write (f, _T("chipset_refreshrate"), _T("%f"), p->chipset_refreshrate);

	for (int i = 0; i < MAX_CHIPSET_REFRESH_TOTAL; i++) {
		if (p->cr[i].rate <= 0)
			continue;
		struct chipset_refresh *cr = &p->cr[i];
		cr->index = i;
		_stprintf (tmp, _T("%f"), cr->rate);
		TCHAR *s = tmp + _tcslen (tmp);
		if (cr->label[0] > 0 && i < MAX_CHIPSET_REFRESH)
			s += _stprintf (s, _T(",t=%s"), cr->label);
		if (cr->horiz > 0)
			s += _stprintf (s, _T(",h=%d"), cr->horiz);
		if (cr->vert > 0)
			s += _stprintf (s, _T(",v=%d"), cr->vert);
		if (cr->locked)
			_tcscat (s, _T(",locked"));
		if (cr->ntsc > 0)
			_tcscat (s, _T(",ntsc"));
		else if (cr->ntsc == 0)
			_tcscat (s, _T(",pal"));
		if (cr->lace > 0)
			_tcscat (s, _T(",lace"));
		else if (cr->lace == 0)
			_tcscat (s, _T(",nlace"));
		if (cr->framelength > 0)
			_tcscat (s, _T(",lof"));
		else if (cr->framelength == 0)
			_tcscat (s, _T(",shf"));
		if (cr->vsync > 0)
			_tcscat (s, _T(",vsync"));
		else if (cr->vsync == 0)
			_tcscat (s, _T(",nvsync"));
		if (cr->rtg)
			_tcscat (s, _T(",rtg"));
		if (cr->commands[0]) {
			_tcscat (s, _T(","));
			_tcscat (s, cr->commands);
			for (int j = 0; j < _tcslen (s); j++) {
				if (s[j] == '\n')
					s[j] = ',';
			}
			s[_tcslen (s) - 1] = 0;
		}
		if (i == CHIPSET_REFRESH_PAL)
			cfgfile_dwrite (f, _T("displaydata_pal"), tmp);
		else if (i == CHIPSET_REFRESH_NTSC)
			cfgfile_dwrite (f, _T("displaydata_ntsc"), tmp);
		else
			cfgfile_dwrite (f, _T("displaydata"), tmp);
	}

	cfgfile_write_str (f, _T("collision_level"), collmode[p->collision_level]);

	cfgfile_write_str(f, _T("chipset_compatible"), cscompa[p->cs_compatible]);
	cfgfile_dwrite_str (f, _T("ciaatod"), ciaatodmode[p->cs_ciaatod]);
	cfgfile_dwrite_str (f, _T("rtc"), rtctype[p->cs_rtc]);
	//cfgfile_dwrite (f, _T("chipset_rtc_adjust"), _T("%d"), p->cs_rtc_adjust);
	cfgfile_dwrite_bool (f, _T("ksmirror_e0"), p->cs_ksmirror_e0);
	cfgfile_dwrite_bool (f, _T("ksmirror_a8"), p->cs_ksmirror_a8);
	cfgfile_dwrite_bool (f, _T("cd32cd"), p->cs_cd32cd);
	cfgfile_dwrite_bool (f, _T("cd32c2p"), p->cs_cd32c2p);
	cfgfile_dwrite_bool(f, _T("cd32nvram"), p->cs_cd32nvram);
	cfgfile_dwrite (f, _T("cd32nvram_size"), _T("%d"), p->cs_cd32nvram_size / 1024);
	cfgfile_dwrite_bool(f, _T("cd32fmv"), p->cs_cd32fmv);
	cfgfile_dwrite_bool(f, _T("cdtvcd"), p->cs_cdtvcd);
	cfgfile_dwrite_bool(f, _T("cdtv-cr"), p->cs_cdtvcr);
	cfgfile_dwrite_bool (f, _T("cdtvram"), p->cs_cdtvram);
	cfgfile_dwrite (f, _T("cdtvramcard"), _T("%d"), p->cs_cdtvcard);
	cfgfile_dwrite_str (f, _T("ide"), p->cs_ide == IDE_A600A1200 ? _T("a600/a1200") : (p->cs_ide == IDE_A4000 ? _T("a4000") : _T("none")));
	cfgfile_dwrite_bool (f, _T("a1000ram"), p->cs_a1000ram);
	cfgfile_dwrite (f, _T("fatgary"), _T("%d"), p->cs_fatgaryrev);
	cfgfile_dwrite (f, _T("ramsey"), _T("%d"), p->cs_ramseyrev);
	cfgfile_dwrite_bool (f, _T("pcmcia"), p->cs_pcmcia);
	cfgfile_dwrite_bool (f, _T("scsi_cdtv"), p->cs_cdtvscsi);
	cfgfile_dwrite_bool (f, _T("scsi_a3000"), p->cs_mbdmac == 1);
	cfgfile_dwrite_bool (f, _T("scsi_a4000t"), p->cs_mbdmac == 2);
	cfgfile_dwrite_bool (f, _T("bogomem_fast"), p->cs_slowmemisfast);
	cfgfile_dwrite_bool (f, _T("resetwarning"), p->cs_resetwarning);
	cfgfile_dwrite_bool (f, _T("denise_noehb"), p->cs_denisenoehb);
	cfgfile_dwrite_bool (f, _T("agnus_bltbusybug"), p->cs_agnusbltbusybug);
	cfgfile_dwrite_bool (f, _T("ics_agnus"), p->cs_dipagnus);
	cfgfile_dwrite_bool (f, _T("cia_todbug"), p->cs_ciatodbug);
	cfgfile_dwrite_bool (f, _T("z3_autoconfig"), p->cs_z3autoconfig);
	cfgfile_dwrite_bool (f, _T("1mchipjumper"), p->cs_1mchipjumper);
	cfgfile_dwrite (f, _T("chipset_hacks"), _T("0x%x"), p->cs_hacks);

	cfgfile_dwrite_str (f, _T("z3mapping"), z3mapping[p->z3_mapping_mode]);
	cfgfile_dwrite_bool (f, _T("fastmem_autoconfig"), p->fastmem_autoconfig);
	if (p->fastmem_size < 0x100000 && p->fastmem_size)
		cfgfile_write (f, _T("fastmem_size_k"), _T("%d"), p->fastmem_size / 1024);
	else
		cfgfile_write (f, _T("fastmem_size"), _T("%d"), p->fastmem_size / 0x100000);
	if (p->fastmem2_size < 0x100000 && p->fastmem2_size)
		cfgfile_dwrite (f, _T("fastmem2_size_k"), _T("%d"), p->fastmem2_size / 1024);
	else
		cfgfile_dwrite (f, _T("fastmem2_size"), _T("%d"), p->fastmem2_size / 0x100000);
	cfgfile_write (f, _T("mem25bit_size"), _T("%d"), p->mem25bit_size / 0x100000);
	cfgfile_write (f, _T("a3000mem_size"), _T("%d"), p->mbresmem_low_size / 0x100000);
	cfgfile_write (f, _T("mbresmem_size"), _T("%d"), p->mbresmem_high_size / 0x100000);
	cfgfile_write (f, _T("z3mem_size"), _T("%d"), p->z3fastmem_size / 0x100000);
	cfgfile_dwrite (f, _T("z3mem2_size"), _T("%d"), p->z3fastmem2_size / 0x100000);
	cfgfile_write (f, _T("z3mem_start"), _T("0x%x"), p->z3autoconfig_start);
	cfgfile_write(f, _T("bogomem_size"), _T("%d"), p->bogomem_size / 0x40000);
	if (p->cpuboard_type) {
		const struct cpuboardtype *cbt = &cpuboards[p->cpuboard_type];
		const struct cpuboardsubtype *cbst = &cbt->subtypes[p->cpuboard_subtype];
		const struct expansionboardsettings *cbs = cbst->settings;
		cfgfile_dwrite_str(f, _T("cpuboard_type"), cbst->configname);
		if (cbs && p->cpuboard_settings) {
			tmp[0] = 0;
			for (int i = 0; cbs[i].name; i++) {
				if (p->cpuboard_settings & (1 << i)) {
					if (tmp[0])
						_tcscat(tmp, _T(","));
					_tcscat(tmp, cbs[i].configname);
				}
			}
			cfgfile_dwrite_str(f, _T("cpuboard_settings"), tmp);
		}
	} else {
		cfgfile_dwrite_str(f, _T("cpuboard_type"), _T("none"));
	}
	cfgfile_dwrite(f, _T("cpuboardmem1_size"), _T("%d"), p->cpuboardmem1_size / 0x100000);
	cfgfile_dwrite(f, _T("cpuboardmem2_size"), _T("%d"), p->cpuboardmem2_size / 0x100000);
	cfgfile_write(f, _T("gfxcard_size"), _T("%d"), p->rtgmem_size / 0x100000);
	cfgfile_write_str(f, _T("gfxcard_type"), gfxboard_get_configname(p->rtgmem_type));
	cfgfile_write_bool(f, _T("gfxcard_hardware_vblank"), p->rtg_hardwareinterrupt);
	cfgfile_write_bool (f, _T("gfxcard_hardware_sprite"), p->rtg_hardwaresprite);
	cfgfile_write (f, _T("chipmem_size"), _T("%d"), p->chipmem_size == 0x20000 ? -1 : (p->chipmem_size == 0x40000 ? 0 : p->chipmem_size / 0x80000));
	cfgfile_dwrite (f, _T("megachipmem_size"), _T("%d"), p->z3chipmem_size / 0x100000);
	// do not save aros rom special space
	if (!(p->custom_memory_sizes[0] == 512 * 1024 && p->custom_memory_sizes[1] == 512 * 1024 && p->custom_memory_addrs[0] == 0xa80000 && p->custom_memory_addrs[1] == 0xb00000)) {
		if (p->custom_memory_sizes[0])
			cfgfile_write (f, _T("addmem1"), _T("0x%x,0x%x"), p->custom_memory_addrs[0], p->custom_memory_sizes[0]);
		if (p->custom_memory_sizes[1])
			cfgfile_write (f, _T("addmem2"), _T("0x%x,0x%x"), p->custom_memory_addrs[1], p->custom_memory_sizes[1]);
	}

	if (p->m68k_speed > 0) {
		cfgfile_write (f, _T("finegrain_cpu_speed"), _T("%d"), p->m68k_speed);
	} else {
		cfgfile_write_str (f, _T("cpu_speed"), p->m68k_speed < 0 ? _T("max") : _T("real"));
	}
	cfgfile_write (f, _T("cpu_throttle"), _T("%.1f"), p->m68k_speed_throttle);

	/* do not reorder start */
	write_compatibility_cpu(f, p);
	cfgfile_write (f, _T("cpu_model"), _T("%d"), p->cpu_model);
	if (p->fpu_model)
		cfgfile_write (f, _T("fpu_model"), _T("%d"), p->fpu_model);
	if (p->mmu_model)
		cfgfile_write (f, _T("mmu_model"), _T("%d"), p->mmu_model);
	if (p->ppc_mode) {
		cfgfile_write_str(f, _T("ppc_model"), p->ppc_model[0] ? p->ppc_model : (p->ppc_mode == 1 ? _T("automatic") : _T("manual")));
		cfgfile_write_str(f, _T("ppc_cpu_idle"), ppc_cpu_idle[p->ppc_cpu_idle]);
	}
	cfgfile_write_bool (f, _T("cpu_compatible"), p->cpu_compatible);
	cfgfile_write_bool (f, _T("cpu_24bit_addressing"), p->address_space_24);
	/* do not reorder end */
	cfgfile_dwrite_bool(f, _T("cpu_reset_pause"), p->reset_delay);
	if (p->ppc_mode)
		cfgfile_write_str(f, _T("ppc_implementation"), ppc_implementations[p->ppc_implementation]);

	if (p->cpu_cycle_exact) {
		if (p->cpu_frequency)
			cfgfile_write (f, _T("cpu_frequency"), _T("%d"), p->cpu_frequency);
		if (p->cpu_clock_multiplier) {
			if (p->cpu_clock_multiplier >= 256)
				cfgfile_write (f, _T("cpu_multiplier"), _T("%d"), p->cpu_clock_multiplier >> 8);
		}
	}

	cfgfile_write_bool (f, _T("cpu_cycle_exact"), p->cpu_cycle_exact);
	cfgfile_write_bool (f, _T("blitter_cycle_exact"), p->blitter_cycle_exact);
	cfgfile_write_bool (f, _T("cycle_exact"), p->cpu_cycle_exact && p->blitter_cycle_exact ? 1 : 0);
	cfgfile_dwrite_bool (f, _T("fpu_no_unimplemented"), p->fpu_no_unimplemented);
	cfgfile_dwrite_bool (f, _T("cpu_no_unimplemented"), p->int_no_unimplemented);
	cfgfile_write_bool (f, _T("fpu_strict"), p->fpu_strict);
	cfgfile_dwrite_bool (f, _T("fpu_softfloat"), p->fpu_softfloat);

	cfgfile_write_bool (f, _T("rtg_nocustom"), p->picasso96_nocustom);
	cfgfile_write (f, _T("rtg_modes"), _T("0x%x"), p->picasso96_modeflags);

	cfgfile_write_bool (f, _T("log_illegal_mem"), p->illegal_mem);
	if (p->catweasel >= 100)
		cfgfile_dwrite (f, _T("catweasel"), _T("0x%x"), p->catweasel);
	else
		cfgfile_dwrite (f, _T("catweasel"), _T("%d"), p->catweasel);
	cfgfile_write_bool(f, _T("toccata"), p->sound_toccata);
	if (p->sound_toccata_mixer)
		cfgfile_write_bool(f, _T("toccata_mixer"), p->sound_toccata_mixer);
	cfgfile_write_bool(f, _T("es1370_pci"), p->sound_es1370);
	cfgfile_write_bool(f, _T("fm801_pci"), p->sound_fm801);

	cfgfile_write_str (f, _T("kbd_lang"), (p->keyboard_lang == KBD_LANG_DE ? _T("de")
		: p->keyboard_lang == KBD_LANG_DK ? _T("dk")
		: p->keyboard_lang == KBD_LANG_ES ? _T("es")
		: p->keyboard_lang == KBD_LANG_US ? _T("us")
		: p->keyboard_lang == KBD_LANG_SE ? _T("se")
		: p->keyboard_lang == KBD_LANG_FR ? _T("fr")
		: p->keyboard_lang == KBD_LANG_IT ? _T("it")
		: _T("FOO")));

	cfgfile_dwrite (f, _T("state_replay_rate"), _T("%d"), p->statecapturerate);
	cfgfile_dwrite (f, _T("state_replay_buffers"), _T("%d"), p->statecapturebuffersize);
	cfgfile_dwrite_bool (f, _T("state_replay_autoplay"), p->inprec_autoplay);
	cfgfile_dwrite_bool (f, _T("warp"), p->turbo_emulation);

#ifdef FILESYS
	write_filesys_config (p, f);
	if (p->filesys_no_uaefsdb)
		cfgfile_write_bool (f, _T("filesys_no_fsdb"), p->filesys_no_uaefsdb);
	cfgfile_dwrite (f, _T("filesys_max_size"), _T("%d"), p->filesys_limit);
	cfgfile_dwrite (f, _T("filesys_max_name_length"), _T("%d"), p->filesys_max_name);
	cfgfile_dwrite (f, _T("filesys_max_file_size"), _T("%d"), p->filesys_max_file_size);
	cfgfile_dwrite_bool (f, _T("filesys_inject_icons"), p->filesys_inject_icons);
	cfgfile_dwrite_str (f, _T("filesys_inject_icons_drawer"), p->filesys_inject_icons_drawer);
	cfgfile_dwrite_str (f, _T("filesys_inject_icons_project"), p->filesys_inject_icons_project);
	cfgfile_dwrite_str (f, _T("filesys_inject_icons_tool"), p->filesys_inject_icons_tool);
	cfgfile_dwrite_str (f, _T("scsidev_mode"), uaescsidevmodes[p->uaescsidevmode]);

#endif
	write_inputdevice_config (p, f);
}

int cfgfile_yesno (const TCHAR *option, const TCHAR *value, const TCHAR *name, int *location, bool numbercheck)
{
	if (name != NULL && _tcscmp (option, name) != 0)
		return 0;
	if (strcasecmp (value, _T("yes")) == 0 || strcasecmp (value, _T("y")) == 0
		|| strcasecmp (value, _T("true")) == 0 || strcasecmp (value, _T("t")) == 0)
		*location = 1;
	else if (strcasecmp (value, _T("no")) == 0 || strcasecmp (value, _T("n")) == 0
		|| strcasecmp (value, _T("false")) == 0 || strcasecmp (value, _T("f")) == 0
		|| (numbercheck && strcasecmp (value, _T("0")) == 0))
		*location = 0;
	else {
		write_log (_T("Option `%s' requires a value of either `yes' or `no' (was '%s').\n"), option, value);
		return -1;
	}
	return 1;
}
int cfgfile_yesno (const TCHAR *option, const TCHAR *value, const TCHAR *name, int *location)
{
	return cfgfile_yesno (option, value, name, location, true);
}
int cfgfile_yesno (const TCHAR *option, const TCHAR *value, const TCHAR *name, bool *location, bool numbercheck)
{
	int val;
	int ret = cfgfile_yesno (option, value, name, &val, numbercheck);
	if (ret == 0)
		return 0;
	if (ret < 0)
		*location = false;
	else
		*location = val != 0;
	return 1;
}
int cfgfile_yesno (const TCHAR *option, const TCHAR *value, const TCHAR *name, bool *location)
{
	return cfgfile_yesno (option, value, name, location, true);
}

int cfgfile_doubleval (const TCHAR *option, const TCHAR *value, const TCHAR *name, double *location)
{
	int base = 10;
	TCHAR *endptr;
	if (name != NULL && _tcscmp (option, name) != 0)
		return 0;
	*location = _tcstod (value, &endptr);
	return 1;
}

int cfgfile_floatval (const TCHAR *option, const TCHAR *value, const TCHAR *name, const TCHAR *nameext, float *location)
{
	int base = 10;
	TCHAR *endptr;
	if (name == NULL)
		return 0;
	if (nameext) {
		TCHAR tmp[MAX_DPATH];
		_tcscpy (tmp, name);
		_tcscat (tmp, nameext);
		if (_tcscmp (tmp, option) != 0)
			return 0;
	} else {
		if (_tcscmp (option, name) != 0)
			return 0;
	}
	*location = (float)_tcstod (value, &endptr);
	return 1;
}
int cfgfile_floatval (const TCHAR *option, const TCHAR *value, const TCHAR *name, float *location)
{
	return cfgfile_floatval (option, value, name, NULL, location);
}

int cfgfile_intval (const TCHAR *option, const TCHAR *value, const TCHAR *name, const TCHAR *nameext, unsigned int *location, int scale)
{
	int base = 10;
	TCHAR *endptr;
	TCHAR tmp[MAX_DPATH];

	if (name == NULL)
		return 0;
	if (nameext) {
		_tcscpy (tmp, name);
		_tcscat (tmp, nameext);
		if (_tcscmp (tmp, option) != 0)
			return 0;
	} else {
		if (_tcscmp (option, name) != 0)
			return 0;
	}
	/* I guess octal isn't popular enough to worry about here...  */
	if (value[0] == '0' && _totupper (value[1]) == 'X')
		value += 2, base = 16;
	*location = _tcstol (value, &endptr, base) * scale;

	if (*endptr != '\0' || *value == '\0') {
		if (strcasecmp (value, _T("false")) == 0 || strcasecmp (value, _T("no")) == 0) {
			*location = 0;
			return 1;
		}
		if (strcasecmp (value, _T("true")) == 0 || strcasecmp (value, _T("yes")) == 0) {
			*location = 1;
			return 1;
		}
		write_log (_T("Option '%s' requires a numeric argument but got '%s'\n"), nameext ? tmp : option, value);
		return -1;
	}
	return 1;
}
int cfgfile_intval (const TCHAR *option, const TCHAR *value, const TCHAR *name, unsigned int *location, int scale)
{
	return cfgfile_intval (option, value, name, NULL, location, scale);
}
int cfgfile_intval (const TCHAR *option, const TCHAR *value, const TCHAR *name, int *location, int scale)
{
	unsigned int v = 0;
	int r = cfgfile_intval (option, value, name, NULL, &v, scale);
	if (!r)
		return 0;
	*location = (int)v;
	return r;
}
int cfgfile_intval (const TCHAR *option, const TCHAR *value, const TCHAR *name, const TCHAR *nameext, int *location, int scale)
{
	unsigned int v = 0;
	int r = cfgfile_intval (option, value, name, nameext, &v, scale);
	if (!r)
		return 0;
	*location = (int)v;
	return r;
}

int cfgfile_strval (const TCHAR *option, const TCHAR *value, const TCHAR *name, const TCHAR *nameext, int *location, const TCHAR *table[], int more)
{
	int val;
	TCHAR tmp[MAX_DPATH];
	if (name == NULL)
		return 0;
	if (nameext) {
		_tcscpy (tmp, name);
		_tcscat (tmp, nameext);
		if (_tcscmp (tmp, option) != 0)
			return 0;
	} else {
		if (_tcscmp (option, name) != 0)
			return 0;
	}
	val = match_string (table, value);
	if (val == -1) {
		if (more)
			return 0;
		if (!strcasecmp (value, _T("yes")) || !strcasecmp (value, _T("true"))) {
			val = 1;
		} else if  (!strcasecmp (value, _T("no")) || !strcasecmp (value, _T("false"))) {
			val = 0;
		} else {
			write_log (_T("Unknown value ('%s') for option '%s'.\n"), value, nameext ? tmp : option);
			return -1;
		}
	}
	*location = val;
	return 1;
}
int cfgfile_strval (const TCHAR *option, const TCHAR *value, const TCHAR *name, int *location, const TCHAR *table[], int more)
{
	return cfgfile_strval (option, value, name, NULL, location, table, more);
}

int cfgfile_strboolval (const TCHAR *option, const TCHAR *value, const TCHAR *name, bool *location, const TCHAR *table[], int more)
{
	int locationint;
	if (!cfgfile_strval (option, value, name, &locationint, table, more))
		return 0;
	*location = locationint != 0;
	return 1;
}

int cfgfile_string (const TCHAR *option, const TCHAR *value, const TCHAR *name, TCHAR *location, int maxsz)
{
	if (_tcscmp (option, name) != 0)
		return 0;
	_tcsncpy (location, value, maxsz - 1);
	location[maxsz - 1] = '\0';
	return 1;
}
int cfgfile_string (const TCHAR *option, const TCHAR *value, const TCHAR *name, const TCHAR *nameext, TCHAR *location, int maxsz)
{
	if (nameext) {
		TCHAR tmp[MAX_DPATH];
		_tcscpy (tmp, name);
		_tcscat (tmp, nameext);
		if (_tcscmp (tmp, option) != 0)
			return 0;
	} else {
		if (_tcscmp (option, name) != 0)
			return 0;
	}
	_tcsncpy (location, value, maxsz - 1);
	location[maxsz - 1] = '\0';
	return 1;
}


int cfgfile_path (const TCHAR *option, const TCHAR *value, const TCHAR *name, TCHAR *location, int maxsz, struct multipath *mp)
{
	if (!cfgfile_string (option, value, name, location, maxsz))
		return 0;
	TCHAR *s = target_expand_environment (location);
	_tcsncpy (location, s, maxsz - 1);
	location[maxsz - 1] = 0;
	if (mp) {
		for (int i = 0; i < MAX_PATHS; i++) {
			if (mp->path[i][0] && _tcscmp (mp->path[i], _T(".\\")) != 0 && _tcscmp (mp->path[i], _T("./")) != 0 && (location[0] != '/' && location[0] != '\\' && !_tcschr(location, ':'))) {
				TCHAR np[MAX_DPATH];
				_tcscpy (np, mp->path[i]);
				fixtrailing (np);
				_tcscat (np, s);
				fullpath (np, sizeof np / sizeof (TCHAR));
				if (zfile_exists (np)) {
					_tcsncpy (location, np, maxsz - 1);
					location[maxsz - 1] = 0;
					break;
				}
			}
		}
	}
	xfree (s);
	return 1;
}
int cfgfile_path (const TCHAR *option, const TCHAR *value, const TCHAR *name, TCHAR *location, int maxsz)
{
	return cfgfile_path (option, value, name, location, maxsz, NULL);
}

int cfgfile_multipath (const TCHAR *option, const TCHAR *value, const TCHAR *name, struct multipath *mp)
{
	TCHAR tmploc[MAX_DPATH];
	if (!cfgfile_string (option, value, name, tmploc, 256))
		return 0;
	for (int i = 0; i < MAX_PATHS; i++) {
		if (mp->path[i][0] == 0 || (i == 0 && (!_tcscmp (mp->path[i], _T(".\\")) || !_tcscmp (mp->path[i], _T("./"))))) {
			TCHAR *s = target_expand_environment (tmploc);
			_tcsncpy (mp->path[i], s, 256 - 1);
			mp->path[i][256 - 1] = 0;
			fixtrailing (mp->path[i]);
			xfree (s);
			return 1;
		}
	}
	return 1;
}

int cfgfile_rom (const TCHAR *option, const TCHAR *value, const TCHAR *name, TCHAR *location, int maxsz)
{
	TCHAR id[MAX_DPATH];
	if (!cfgfile_string (option, value, name, id, sizeof id / sizeof (TCHAR)))
		return 0;
	TCHAR *p = _tcschr (id, ',');
	if (p) {
		TCHAR *endptr, tmp;
		*p = 0;
		tmp = id[4];
		id[4] = 0;
		uae_u32 crc32 = _tcstol (id, &endptr, 16) << 16;
		id[4] = tmp;
		crc32 |= _tcstol (id + 4, &endptr, 16);
		struct romdata *rd = getromdatabycrc (crc32, true);
		if (rd) {
			struct romdata *rd2 = getromdatabyid (rd->id);
			if (rd->group == 0 && rd2 == rd) {
				if (zfile_exists (location))
					return 1;
			}
			if (rd->group && rd2)
				rd = rd2;
			struct romlist *rl = getromlistbyromdata (rd);
			if (rl) {
				write_log (_T("%s: %s -> %s\n"), name, location, rl->path);
				_tcsncpy (location, rl->path, maxsz);
			}
		}
	}
	return 1;
}

static int getintval (TCHAR **p, int *result, int delim)
{
	TCHAR *value = *p;
	int base = 10;
	TCHAR *endptr;
	TCHAR *p2 = _tcschr (*p, delim);

	if (p2 == 0)
		return 0;

	*p2++ = '\0';

	if (value[0] == '0' && _totupper (value[1]) == 'X')
		value += 2, base = 16;
	*result = _tcstol (value, &endptr, base);
	*p = p2;

	if (*endptr != '\0' || *value == '\0')
		return 0;

	return 1;
}

static int getintval2 (TCHAR **p, int *result, int delim, bool last)
{
	TCHAR *value = *p;
	int base = 10;
	TCHAR *endptr;
	TCHAR *p2;
	
	p2 = _tcschr (*p, delim);
	if (p2 == 0) {
		if (last) {
			if (delim != '.')
				p2 = _tcschr (*p, ',');
			if (p2 == 0) {
				p2 = *p;
				while(*p2)
					p2++;
				if (p2 == *p)
					return 0;
			}
		} else {
			return 0;
		}
	}
	if (!_istdigit(**p) && **p != '-' && **p != '+')
		return 0;

	if (*p2 != 0)
		*p2++ = '\0';

	if (value[0] == '0' && _totupper (value[1]) == 'X')
		value += 2, base = 16;
	*result = _tcstol (value, &endptr, base);
	*p = p2;

	if (*endptr != '\0' || *value == '\0') {
		*p = 0;
		return 0;
	}

	return 1;
}

static TCHAR *cfgfile_option_find_it(TCHAR *s, const TCHAR *option, bool checkequals)
{
	TCHAR buf[MAX_DPATH];
	if (!s)
		return NULL;
	_tcscpy(buf, s);
	_tcscat(buf, _T(","));
	TCHAR *p = buf;
	for (;;) {
		TCHAR *tmpp = _tcschr (p, ',');
		TCHAR *tmpp2 = NULL;
		if (tmpp == NULL)
			return NULL;
		*tmpp++ = 0;
		if (checkequals) {
			tmpp2 = _tcschr(p, '=');
			if (!tmpp2)
				return NULL;
			*tmpp2++ = 0;
		}
		if (!strcasecmp(p, option)) {
			if (checkequals)
				return tmpp2;
			return p;
		}
		p = tmpp;
	}
}

static bool cfgfile_option_find(TCHAR *s, const TCHAR *option)
{
	return cfgfile_option_find_it(s, option, false) != NULL;
}

static TCHAR *cfgfile_option_get(TCHAR *s, const TCHAR *option)
{
	return cfgfile_option_find_it(s, option, true);
}

static int cfgfile_option_select(TCHAR *s, const TCHAR *option, const TCHAR *select)
{
	TCHAR buf[MAX_DPATH];
	if (!s)
		return -1;
	_tcscpy(buf, s);
	_tcscat(buf, _T(","));
	TCHAR *p = buf;
	for (;;) {
		TCHAR *tmpp = _tcschr (p, ',');
		if (tmpp == NULL)
			return -1;
		*tmpp++ = 0;
		TCHAR *tmpp2 = _tcschr(p, '=');
		if (!tmpp2)
			return -1;
		*tmpp2++ = 0;
		if (!strcasecmp(p, option)) {
			int idx = 0;
			while (select[0]) {
				if (!strcasecmp(select, tmpp2))
					return idx;
				idx++;
				select += _tcslen(select) + 1;
			}
		}
		p = tmpp;
	}
}

static int cfgfile_option_bool(TCHAR *s, const TCHAR *option)
{
	TCHAR buf[MAX_DPATH];
	if (!s)
		return -1;
	_tcscpy(buf, s);
	_tcscat(buf, _T(","));
	TCHAR *p = buf;
	for (;;) {
		TCHAR *tmpp = _tcschr (p, ',');
		if (tmpp == NULL)
			return -1;
		*tmpp++ = 0;
		TCHAR *tmpp2 = _tcschr(p, '=');
		if (tmpp2)
			*tmpp2++ = 0;
		if (!strcasecmp(p, option)) {
			if (!tmpp2)
				return 0;
			TCHAR *tmpp3 = _tcschr (tmpp2, ',');
			if (tmpp3)
				*tmpp3 = 0;
			if (tmpp2 && !strcasecmp(tmpp2, _T("true")))
				return 1;
			if (tmpp2 && !strcasecmp(tmpp2, _T("false")))
				return 0;
			return 1;
		}
		p = tmpp;
	}
}
static void set_chipset_mask (struct uae_prefs *p, int val)
{
	p->chipset_mask = (val == 0 ? 0
		: val == 1 ? CSMASK_ECS_AGNUS
		: val == 2 ? CSMASK_ECS_DENISE
		: val == 3 ? CSMASK_ECS_DENISE | CSMASK_ECS_AGNUS
		: CSMASK_AGA | CSMASK_ECS_DENISE | CSMASK_ECS_AGNUS);
}

static int cfgfile_parse_host (struct uae_prefs *p, TCHAR *option, TCHAR *value)
{
	int i, v;
	bool vb;
	TCHAR *section = 0;
	TCHAR *tmpp;
	TCHAR tmpbuf[CONFIG_BLEN];

	if (_tcsncmp (option, _T("input."), 6) == 0) {
		read_inputdevice_config (p, option, value);
		return 1;
	}

	for (tmpp = option; *tmpp != '\0'; tmpp++)
		if (_istupper (*tmpp))
			*tmpp = _totlower (*tmpp);
	tmpp = _tcschr (option, '.');
	if (tmpp) {
		section = option;
		option = tmpp + 1;
		*tmpp = '\0';
		if (_tcscmp (section, TARGET_NAME) == 0) {
			/* We special case the various path options here.  */
			if (cfgfile_multipath (option, value, _T("rom_path"), &p->path_rom)
				|| cfgfile_multipath (option, value, _T("floppy_path"), &p->path_floppy)
				|| cfgfile_multipath (option, value, _T("cd_path"), &p->path_cd)
				|| cfgfile_multipath (option, value, _T("hardfile_path"), &p->path_hardfile))
				return 1;
			return target_parse_option (p, option, value);
		}
		return 0;
	}

	for (i = 0; i < MAX_SPARE_DRIVES; i++) {
		_stprintf (tmpbuf, _T("diskimage%d"), i);
		if (cfgfile_path (option, value, tmpbuf, p->dfxlist[i], sizeof p->dfxlist[i] / sizeof (TCHAR), &p->path_floppy)) {
#if 0
			if (i < 4 && !p->df[i][0])
				_tcscpy (p->df[i], p->dfxlist[i]);
#endif
			return 1;
		}
	}

	for (i = 0; i < MAX_TOTAL_SCSI_DEVICES; i++) {
		TCHAR tmp[20];
		_stprintf (tmp, _T("cdimage%d"), i);
		if (!_tcsicmp (option, tmp)) {
			if (!_tcsicmp (value, _T("autodetect"))) {
				p->cdslots[i].type = SCSI_UNIT_DEFAULT;
				p->cdslots[i].inuse = true;
				p->cdslots[i].name[0] = 0;
			} else {
				p->cdslots[i].delayed = false;
				TCHAR *next = _tcsrchr (value, ',');
				int type = SCSI_UNIT_DEFAULT;
				int mode = 0;
				int unitnum = 0;
				for (;;) {
					if (!next)
						break;
					*next++ = 0;
					TCHAR *next2 = _tcschr (next, ':');
					if (next2)
						*next2++ = 0;
					int tmpval = 0;
					if (!_tcsicmp (next, _T("delay"))) {
						p->cdslots[i].delayed = true;
						next = next2;
						if (!next)
							break;
						next2 = _tcschr (next, ':');
						if (next2)
							*next2++ = 0;
					}
					type = match_string (cdmodes, next);
					if (type < 0)
						type = SCSI_UNIT_DEFAULT;
					else
						type--;
					next = next2;
					if (!next)
						break;
					next2 = _tcschr (next, ':');
					if (next2)
						*next2++ = 0;
					mode = match_string (cdconmodes, next);
					if (mode < 0)
						mode = 0;
					next = next2;
					if (!next)
						break;
					next2 = _tcschr (next, ':');
					if (next2)
						*next2++ = 0;
					cfgfile_intval (option, next, tmp, &unitnum, 1);
				}
				if (_tcslen (value) > 0) {
					TCHAR *s = cfgfile_get_multipath (&p->path_cd, NULL, value, false);
					_tcsncpy (p->cdslots[i].name, s, sizeof p->cdslots[i].name / sizeof (TCHAR));
					xfree (s);
				}
				p->cdslots[i].name[sizeof p->cdslots[i].name - 1] = 0;
				p->cdslots[i].inuse = true;
				p->cdslots[i].type = type;
			}
			// disable all following units
			i++;
			while (i < MAX_TOTAL_SCSI_DEVICES) {
				p->cdslots[i].type = SCSI_UNIT_DISABLED;
				i++;
			}
			return 1;
		}
	}

	if (!_tcsicmp (option, _T("lua"))) {
		for (i = 0; i < MAX_LUA_STATES; i++) {
			if (!p->luafiles[i][0]) {
				_tcscpy (p->luafiles[i], value);
				break;
			}
		}
		return 1;
	}
		
	if (cfgfile_strval (option, value, _T("gfx_autoresolution_min_vertical"), &p->gfx_autoresolution_minv, vertmode, 0)) {
		p->gfx_autoresolution_minv--;
		return 1;
	}
	if (cfgfile_strval (option, value, _T("gfx_autoresolution_min_horizontal"), &p->gfx_autoresolution_minh, horizmode, 0)) {
		p->gfx_autoresolution_minh--;
		return 1;
	}
	if (!_tcsicmp (option, _T("gfx_autoresolution"))) {
		p->gfx_autoresolution = 0;
		cfgfile_intval (option, value, _T("gfx_autoresolution"), &p->gfx_autoresolution, 1);
		if (!p->gfx_autoresolution) {
			v = cfgfile_yesno (option, value, _T("gfx_autoresolution"), &vb);
			if (v > 0)
				p->gfx_autoresolution = vb ? 10 : 0;
		}
		return 1;
	}

	if (cfgfile_intval (option, value, _T("sound_frequency"), &p->sound_freq, 1)
		|| cfgfile_intval (option, value, _T("sound_max_buff"), &p->sound_maxbsiz, 1)
		|| cfgfile_intval (option, value, _T("state_replay_rate"), &p->statecapturerate, 1)
		|| cfgfile_intval (option, value, _T("state_replay_buffers"), &p->statecapturebuffersize, 1)
		|| cfgfile_yesno (option, value, _T("state_replay_autoplay"), &p->inprec_autoplay)
		|| cfgfile_intval (option, value, _T("sound_frequency"), &p->sound_freq, 1)
		|| cfgfile_intval (option, value, _T("sound_volume"), &p->sound_volume_master, 1)
		|| cfgfile_intval (option, value, _T("sound_volume_paula"), &p->sound_volume_paula, 1)
		|| cfgfile_intval (option, value, _T("sound_volume_cd"), &p->sound_volume_cd, 1)
		|| cfgfile_intval (option, value, _T("sound_volume_ahi"), &p->sound_volume_board, 1)
		|| cfgfile_intval (option, value, _T("sound_stereo_separation"), &p->sound_stereo_separation, 1)
		|| cfgfile_intval (option, value, _T("sound_stereo_mixing_delay"), &p->sound_mixed_stereo_delay, 1)
		|| cfgfile_intval (option, value, _T("sampler_frequency"), &p->sampler_freq, 1)
		|| cfgfile_intval (option, value, _T("sampler_buffer"), &p->sampler_buffer, 1)

		|| cfgfile_intval (option, value, _T("gfx_framerate"), &p->gfx_framerate, 1)
		|| cfgfile_intval (option, value, _T("gfx_top_windowed"), &p->gfx_size_win.x, 1)
		|| cfgfile_intval (option, value, _T("gfx_left_windowed"), &p->gfx_size_win.y, 1)
		|| cfgfile_intval (option, value, _T("gfx_refreshrate"), &p->gfx_apmode[APMODE_NATIVE].gfx_refreshrate, 1)
		|| cfgfile_intval (option, value, _T("gfx_refreshrate_rtg"), &p->gfx_apmode[APMODE_RTG].gfx_refreshrate, 1)
		|| cfgfile_intval (option, value, _T("gfx_autoresolution_delay"), &p->gfx_autoresolution_delay, 1)
		|| cfgfile_intval (option, value, _T("gfx_backbuffers"), &p->gfx_apmode[APMODE_NATIVE].gfx_backbuffers, 1)
		|| cfgfile_intval (option, value, _T("gfx_backbuffers_rtg"), &p->gfx_apmode[APMODE_RTG].gfx_backbuffers, 1)
		|| cfgfile_yesno (option, value, _T("gfx_interlace"), &p->gfx_apmode[APMODE_NATIVE].gfx_interlaced)
		|| cfgfile_yesno (option, value, _T("gfx_interlace_rtg"), &p->gfx_apmode[APMODE_RTG].gfx_interlaced)
		
		|| cfgfile_intval (option, value, _T("gfx_center_horizontal_position"), &p->gfx_xcenter_pos, 1)
		|| cfgfile_intval (option, value, _T("gfx_center_vertical_position"), &p->gfx_ycenter_pos, 1)
		|| cfgfile_intval (option, value, _T("gfx_center_horizontal_size"), &p->gfx_xcenter_size, 1)
		|| cfgfile_intval (option, value, _T("gfx_center_vertical_size"), &p->gfx_ycenter_size, 1)

		|| cfgfile_intval (option, value, _T("filesys_max_size"), &p->filesys_limit, 1)
		|| cfgfile_intval (option, value, _T("filesys_max_name_length"), &p->filesys_max_name, 1)
		|| cfgfile_intval (option, value, _T("filesys_max_file_size"), &p->filesys_max_file_size, 1)
		|| cfgfile_yesno (option, value, _T("filesys_inject_icons"), &p->filesys_inject_icons)
		|| cfgfile_string (option, value, _T("filesys_inject_icons_drawer"), p->filesys_inject_icons_drawer, sizeof p->filesys_inject_icons_drawer / sizeof (TCHAR))
		|| cfgfile_string (option, value, _T("filesys_inject_icons_project"), p->filesys_inject_icons_project, sizeof p->filesys_inject_icons_project / sizeof (TCHAR))
		|| cfgfile_string (option, value, _T("filesys_inject_icons_tool"), p->filesys_inject_icons_tool, sizeof p->filesys_inject_icons_tool / sizeof (TCHAR))

		|| cfgfile_intval (option, value, _T("gfx_luminance"), &p->gfx_luminance, 1)
		|| cfgfile_intval (option, value, _T("gfx_contrast"), &p->gfx_contrast, 1)
		|| cfgfile_intval (option, value, _T("gfx_gamma"), &p->gfx_gamma, 1)
		|| cfgfile_intval (option, value, _T("gfx_gamma_r"), &p->gfx_gamma_ch[0], 1)
		|| cfgfile_intval (option, value, _T("gfx_gamma_g"), &p->gfx_gamma_ch[1], 1)
		|| cfgfile_intval (option, value, _T("gfx_gamma_b"), &p->gfx_gamma_ch[2], 1)
		|| cfgfile_floatval (option, value, _T("rtg_vert_zoom_multf"), &p->rtg_vert_zoom_mult)
		|| cfgfile_floatval (option, value, _T("rtg_horiz_zoom_multf"), &p->rtg_horiz_zoom_mult)
		|| cfgfile_intval (option, value, _T("gfx_horizontal_tweak"), &p->gfx_extrawidth, 1)

		|| cfgfile_intval (option, value, _T("floppy0sound"), &p->floppyslots[0].dfxclick, 1)
		|| cfgfile_intval (option, value, _T("floppy1sound"), &p->floppyslots[1].dfxclick, 1)
		|| cfgfile_intval (option, value, _T("floppy2sound"), &p->floppyslots[2].dfxclick, 1)
		|| cfgfile_intval (option, value, _T("floppy3sound"), &p->floppyslots[3].dfxclick, 1)
		|| cfgfile_intval (option, value, _T("floppy0soundvolume_disk"), &p->dfxclickvolume_disk[0], 1)
		|| cfgfile_intval (option, value, _T("floppy1soundvolume_disk"), &p->dfxclickvolume_disk[1], 1)
		|| cfgfile_intval (option, value, _T("floppy2soundvolume_disk"), &p->dfxclickvolume_disk[2], 1)
		|| cfgfile_intval (option, value, _T("floppy3soundvolume_disk"), &p->dfxclickvolume_disk[3], 1)
		|| cfgfile_intval (option, value, _T("floppy0soundvolume_empty"), &p->dfxclickvolume_empty[0], 1)
		|| cfgfile_intval (option, value, _T("floppy1soundvolume_empty"), &p->dfxclickvolume_empty[1], 1)
		|| cfgfile_intval (option, value, _T("floppy2soundvolume_empty"), &p->dfxclickvolume_empty[2], 1)
		|| cfgfile_intval (option, value, _T("floppy3soundvolume_empty"), &p->dfxclickvolume_empty[3], 1)
		|| cfgfile_intval (option, value, _T("floppy_channel_mask"), &p->dfxclickchannelmask, 1))
		return 1;

	if (cfgfile_path (option, value, _T("floppy0soundext"), p->floppyslots[0].dfxclickexternal, sizeof p->floppyslots[0].dfxclickexternal / sizeof (TCHAR))
		|| cfgfile_path (option, value, _T("floppy1soundext"), p->floppyslots[1].dfxclickexternal, sizeof p->floppyslots[1].dfxclickexternal / sizeof (TCHAR))
		|| cfgfile_path (option, value, _T("floppy2soundext"), p->floppyslots[2].dfxclickexternal, sizeof p->floppyslots[2].dfxclickexternal / sizeof (TCHAR))
		|| cfgfile_path (option, value, _T("floppy3soundext"), p->floppyslots[3].dfxclickexternal, sizeof p->floppyslots[3].dfxclickexternal / sizeof (TCHAR))
		|| cfgfile_string (option, value, _T("config_window_title"), p->config_window_title, sizeof p->config_window_title / sizeof (TCHAR))
		|| cfgfile_string (option, value, _T("config_info"), p->info, sizeof p->info / sizeof (TCHAR))
		|| cfgfile_string (option, value, _T("config_description"), p->description, sizeof p->description / sizeof (TCHAR)))
		return 1;

	if (cfgfile_yesno (option, value, _T("use_debugger"), &p->start_debugger)
		|| cfgfile_yesno (option, value, _T("floppy0wp"), &p->floppyslots[0].forcedwriteprotect)
		|| cfgfile_yesno (option, value, _T("floppy1wp"), &p->floppyslots[1].forcedwriteprotect)
		|| cfgfile_yesno (option, value, _T("floppy2wp"), &p->floppyslots[2].forcedwriteprotect)
		|| cfgfile_yesno (option, value, _T("floppy3wp"), &p->floppyslots[3].forcedwriteprotect)
		|| cfgfile_yesno (option, value, _T("sampler_stereo"), &p->sampler_stereo)
		|| cfgfile_yesno (option, value, _T("sound_auto"), &p->sound_auto)
		|| cfgfile_yesno (option, value, _T("sound_cdaudio"), &p->sound_cdaudio)
		|| cfgfile_yesno (option, value, _T("sound_stereo_swap_paula"), &p->sound_stereo_swap_paula)
		|| cfgfile_yesno (option, value, _T("sound_stereo_swap_ahi"), &p->sound_stereo_swap_ahi)
		|| cfgfile_yesno (option, value, _T("avoid_cmov"), &p->avoid_cmov)
		|| cfgfile_yesno (option, value, _T("log_illegal_mem"), &p->illegal_mem)
		|| cfgfile_yesno (option, value, _T("filesys_no_fsdb"), &p->filesys_no_uaefsdb)
		|| cfgfile_yesno (option, value, _T("gfx_blacker_than_black"), &p->gfx_blackerthanblack)
		|| cfgfile_yesno (option, value, _T("gfx_black_frame_insertion"), &p->lightboost_strobo)
		|| cfgfile_yesno (option, value, _T("gfx_flickerfixer"), &p->gfx_scandoubler)
		|| cfgfile_yesno (option, value, _T("gfx_autoresolution_vga"), &p->gfx_autoresolution_vga)
		|| cfgfile_yesno (option, value, _T("magic_mouse"), &p->input_magic_mouse)
		|| cfgfile_yesno (option, value, _T("warp"), &p->turbo_emulation)
		|| cfgfile_yesno (option, value, _T("headless"), &p->headless)
		|| cfgfile_yesno (option, value, _T("clipboard_sharing"), &p->clipboard_sharing)
		|| cfgfile_yesno (option, value, _T("native_code"), &p->native_code)
		|| cfgfile_yesno (option, value, _T("tablet_library"), &p->tablet_library)
		|| cfgfile_yesno (option, value, _T("bsdsocket_emu"), &p->socket_emu))
		return 1;

	if (cfgfile_strval (option, value, _T("sound_output"), &p->produce_sound, soundmode1, 1)
		|| cfgfile_strval (option, value, _T("sound_output"), &p->produce_sound, soundmode2, 0)
		|| cfgfile_strval (option, value, _T("sound_interpol"), &p->sound_interpol, interpolmode, 0)
		|| cfgfile_strval (option, value, _T("sound_filter"), &p->sound_filter, soundfiltermode1, 0)
		|| cfgfile_strval (option, value, _T("sound_filter_type"), &p->sound_filter_type, soundfiltermode2, 0)
		|| cfgfile_strboolval (option, value, _T("use_gui"), &p->start_gui, guimode1, 1)
		|| cfgfile_strboolval (option, value, _T("use_gui"), &p->start_gui, guimode2, 1)
		|| cfgfile_strboolval (option, value, _T("use_gui"), &p->start_gui, guimode3, 0)
		|| cfgfile_strval (option, value, _T("gfx_resolution"), &p->gfx_resolution, lorestype1, 0)
		|| cfgfile_strval (option, value, _T("gfx_lores"), &p->gfx_resolution, lorestype2, 0)
		|| cfgfile_strval (option, value, _T("gfx_lores_mode"), &p->gfx_lores_mode, loresmode, 0)
		|| cfgfile_strval (option, value, _T("gfx_fullscreen_amiga"), &p->gfx_apmode[APMODE_NATIVE].gfx_fullscreen, fullmodes, 0)
		|| cfgfile_strval (option, value, _T("gfx_fullscreen_picasso"), &p->gfx_apmode[APMODE_RTG].gfx_fullscreen, fullmodes, 0)
		|| cfgfile_strval (option, value, _T("gfx_center_horizontal"), &p->gfx_xcenter, centermode1, 1)
		|| cfgfile_strval (option, value, _T("gfx_center_vertical"), &p->gfx_ycenter, centermode1, 1)
		|| cfgfile_strval (option, value, _T("gfx_center_horizontal"), &p->gfx_xcenter, centermode2, 0)
		|| cfgfile_strval (option, value, _T("gfx_center_vertical"), &p->gfx_ycenter, centermode2, 0)
		|| cfgfile_strval (option, value, _T("gfx_colour_mode"), &p->color_mode, colormode1, 1)
		|| cfgfile_strval (option, value, _T("gfx_colour_mode"), &p->color_mode, colormode2, 0)
		|| cfgfile_strval (option, value, _T("gfx_color_mode"), &p->color_mode, colormode1, 1)
		|| cfgfile_strval (option, value, _T("gfx_color_mode"), &p->color_mode, colormode2, 0)
		|| cfgfile_strval (option, value, _T("gfx_max_horizontal"), &p->gfx_max_horizontal, maxhoriz, 0)
		|| cfgfile_strval (option, value, _T("gfx_max_vertical"), &p->gfx_max_vertical, maxvert, 0)
		|| cfgfile_strval (option, value, _T("gfx_api"), &p->gfx_api, filterapi, 0)
		|| cfgfile_strval (option, value, _T("magic_mousecursor"), &p->input_magic_mouse_cursor, magiccursors, 0)
		|| cfgfile_strval (option, value, _T("absolute_mouse"), &p->input_tablet, abspointers, 0))
		return 1;

#ifdef GFXFILTER
	for (int j = 0; j < 2; j++) {
		struct gfx_filterdata *gf = &p->gf[j];
		const TCHAR *ext = j == 0 ? NULL : _T("_rtg");
		if (cfgfile_strval (option, value, _T("gfx_filter_autoscale"), ext, &gf->gfx_filter_autoscale, j == 0 ? autoscale : autoscale_rtg, 0)
			|| cfgfile_strval (option, value, _T("gfx_filter_keep_aspect"), ext, &gf->gfx_filter_keep_aspect, aspects, 0)
			|| cfgfile_strval (option, value, _T("gfx_filter_autoscale_limit"), ext, &gf->gfx_filter_integerscalelimit, autoscalelimit, 0))
			return 1;

		if (cfgfile_floatval (option, value, _T("gfx_filter_vert_zoomf"), ext, &gf->gfx_filter_vert_zoom)
			|| cfgfile_floatval (option, value, _T("gfx_filter_horiz_zoomf"), ext, &gf->gfx_filter_horiz_zoom)
			|| cfgfile_floatval (option, value, _T("gfx_filter_vert_zoom_multf"), ext, &gf->gfx_filter_vert_zoom_mult)
			|| cfgfile_floatval (option, value, _T("gfx_filter_horiz_zoom_multf"), ext, &gf->gfx_filter_horiz_zoom_mult)
			|| cfgfile_floatval (option, value, _T("gfx_filter_vert_offsetf"), ext, &gf->gfx_filter_vert_offset)
			|| cfgfile_floatval (option, value, _T("gfx_filter_horiz_offsetf"), ext, &gf->gfx_filter_horiz_offset)
			|| cfgfile_intval (option, value, _T("gfx_filter_left_border"), ext, &gf->gfx_filter_left_border, 1)
			|| cfgfile_intval (option, value, _T("gfx_filter_right_border"), ext, &gf->gfx_filter_right_border, 1)
			|| cfgfile_intval (option, value, _T("gfx_filter_top_border"), ext, &gf->gfx_filter_top_border, 1)
			|| cfgfile_intval (option, value, _T("gfx_filter_bottom_border"), ext, &gf->gfx_filter_bottom_border, 1)
			|| cfgfile_intval (option, value, _T("gfx_filter_scanlines"), ext, &gf->gfx_filter_scanlines, 1)
			|| cfgfile_intval (option, value, _T("gfx_filter_scanlinelevel"), ext, &gf->gfx_filter_scanlinelevel, 1)
			|| cfgfile_intval (option, value, _T("gfx_filter_scanlineratio"), ext, &gf->gfx_filter_scanlineratio, 1)
			|| cfgfile_intval (option, value, _T("gfx_filter_luminance"), ext, &gf->gfx_filter_luminance, 1)
			|| cfgfile_intval (option, value, _T("gfx_filter_contrast"), ext, &gf->gfx_filter_contrast, 1)
			|| cfgfile_intval (option, value, _T("gfx_filter_saturation"), ext, &gf->gfx_filter_saturation, 1)
			|| cfgfile_intval (option, value, _T("gfx_filter_gamma"), ext, &gf->gfx_filter_gamma, 1)
			|| cfgfile_intval (option, value, _T("gfx_filter_gamma_r"), ext, &gf->gfx_filter_gamma_ch[0], 1)
			|| cfgfile_intval (option, value, _T("gfx_filter_gamma_g"), ext, &gf->gfx_filter_gamma_ch[1], 1)
			|| cfgfile_intval (option, value, _T("gfx_filter_gamma_b"), ext, &gf->gfx_filter_gamma_ch[2], 1)
			|| cfgfile_intval (option, value, _T("gfx_filter_blur"), ext, &gf->gfx_filter_blur, 1)
			|| cfgfile_intval (option, value, _T("gfx_filter_noise"), ext, &gf->gfx_filter_noise, 1)
			|| cfgfile_intval (option, value, _T("gfx_filter_bilinear"), ext, &gf->gfx_filter_bilinear, 1)
			|| cfgfile_intval (option, value, _T("gfx_filter_keep_autoscale_aspect"), ext, &gf->gfx_filter_keep_autoscale_aspect, 1)
			|| cfgfile_string (option, value, _T("gfx_filter_mask"), ext, gf->gfx_filtermask[2 * MAX_FILTERSHADERS], sizeof gf->gfx_filtermask[2 * MAX_FILTERSHADERS] / sizeof (TCHAR)))
			return 1;
	}
#endif

	if (cfgfile_intval (option, value, _T("floppy_volume"), &v, 1)) {
		for (int i = 0; i < 4; i++) {
			p->dfxclickvolume_disk[i] = v;
			p->dfxclickvolume_empty[i] = v;
		}
		return 1;
	}

	if (_tcscmp (option, _T("gfx_width_windowed")) == 0) {
		if (!_tcscmp (value, _T("native"))) {
			p->gfx_size_win.width = 0;
			p->gfx_size_win.height = 0;
		} else {
			cfgfile_intval (option, value, _T("gfx_width_windowed"), &p->gfx_size_win.width, 1);
		}
		return 1;
	}
	if (_tcscmp (option, _T("gfx_height_windowed")) == 0) {
		if (!_tcscmp (value, _T("native"))) {
			p->gfx_size_win.width = 0;
			p->gfx_size_win.height = 0;
		} else {
			cfgfile_intval (option, value, _T("gfx_height_windowed"), &p->gfx_size_win.height, 1);
		}
		return 1;
	}
	if (_tcscmp (option, _T("gfx_width_fullscreen")) == 0) {
		if (!_tcscmp (value, _T("native"))) {
			p->gfx_size_fs.width = 0;
			p->gfx_size_fs.height = 0;
			p->gfx_size_fs.special = WH_NATIVE;
		} else {
			cfgfile_intval (option, value, _T("gfx_width_fullscreen"), &p->gfx_size_fs.width, 1);
			p->gfx_size_fs.special = 0;
		}
		return 1;
	}
	if (_tcscmp (option, _T("gfx_height_fullscreen")) == 0) {
		if (!_tcscmp (value, _T("native"))) {
			p->gfx_size_fs.width = 0;
			p->gfx_size_fs.height = 0;
			p->gfx_size_fs.special = WH_NATIVE;
		} else {
			cfgfile_intval (option, value, _T("gfx_height_fullscreen"), &p->gfx_size_fs.height, 1);
			p->gfx_size_fs.special = 0;
		}
		return 1;
	}

	if (cfgfile_intval (option, value, _T("gfx_display"), &p->gfx_apmode[APMODE_NATIVE].gfx_display, 1)) {
		p->gfx_apmode[APMODE_RTG].gfx_display = p->gfx_apmode[APMODE_NATIVE].gfx_display;
		return 1;
	}
	if (cfgfile_intval (option, value, _T("gfx_display_rtg"), &p->gfx_apmode[APMODE_RTG].gfx_display, 1)) {
		return 1;
	}
	if (_tcscmp (option, _T("gfx_display_friendlyname")) == 0 || _tcscmp (option, _T("gfx_display_name")) == 0) {
		TCHAR tmp[MAX_DPATH];
		if (cfgfile_string (option, value, _T("gfx_display_friendlyname"), tmp, sizeof tmp / sizeof (TCHAR))) {
			int num = target_get_display (tmp);
			if (num >= 0)
				p->gfx_apmode[APMODE_RTG].gfx_display = p->gfx_apmode[APMODE_NATIVE].gfx_display = num;
		}
		if (cfgfile_string (option, value, _T("gfx_display_name"), tmp, sizeof tmp / sizeof (TCHAR))) {
			int num = target_get_display (tmp);
			if (num >= 0)
				p->gfx_apmode[APMODE_RTG].gfx_display = p->gfx_apmode[APMODE_NATIVE].gfx_display = num;
		}
		return 1;
	}
	if (_tcscmp (option, _T("gfx_display_friendlyname_rtg")) == 0 || _tcscmp (option, _T("gfx_display_name_rtg")) == 0) {
		TCHAR tmp[MAX_DPATH];
		if (cfgfile_string (option, value, _T("gfx_display_friendlyname_rtg"), tmp, sizeof tmp / sizeof (TCHAR))) {
			int num = target_get_display (tmp);
			if (num >= 0)
				p->gfx_apmode[APMODE_RTG].gfx_display = num;
		}
		if (cfgfile_string (option, value, _T("gfx_display_name_rtg"), tmp, sizeof tmp / sizeof (TCHAR))) {
			int num = target_get_display (tmp);
			if (num >= 0)
				p->gfx_apmode[APMODE_RTG].gfx_display = num;
		}
		return 1;
	}

	if (_tcscmp (option, _T("gfx_linemode")) == 0) {
		int v;
		p->gfx_vresolution = VRES_DOUBLE;
		p->gfx_pscanlines = 0;
		p->gfx_iscanlines = 0;
		if (cfgfile_strval(option, value, _T("gfx_linemode"), &v, linemode, 0)) {
			p->gfx_vresolution = VRES_NONDOUBLE;
			if (v > 0) {
				p->gfx_iscanlines = (v - 1) / 4;
				p->gfx_pscanlines = (v - 1) % 4;
				p->gfx_vresolution = VRES_DOUBLE;
			}
		}
		return 1;
	}
	if (_tcscmp (option, _T("gfx_vsync")) == 0) {
		if (cfgfile_strval (option, value, _T("gfx_vsync"), &p->gfx_apmode[APMODE_NATIVE].gfx_vsync, vsyncmodes, 0) >= 0)
			return 1;
		return cfgfile_yesno (option, value, _T("gfx_vsync"), &p->gfx_apmode[APMODE_NATIVE].gfx_vsync);
	}
	if (_tcscmp (option, _T("gfx_vsync_picasso")) == 0) {
		if (cfgfile_strval (option, value, _T("gfx_vsync_picasso"), &p->gfx_apmode[APMODE_RTG].gfx_vsync, vsyncmodes, 0) >= 0)
			return 1;
		return cfgfile_yesno (option, value, _T("gfx_vsync_picasso"), &p->gfx_apmode[APMODE_RTG].gfx_vsync);
	}
	if (cfgfile_strval (option, value, _T("gfx_vsyncmode"), &p->gfx_apmode[APMODE_NATIVE].gfx_vsyncmode, vsyncmodes2, 0))
		return 1;
	if (cfgfile_strval (option, value, _T("gfx_vsyncmode_picasso"), &p->gfx_apmode[APMODE_RTG].gfx_vsyncmode, vsyncmodes2, 0))
		return 1;

	if (cfgfile_yesno (option, value, _T("show_leds"), &vb)) {
		if (vb)
			p->leds_on_screen |= STATUSLINE_CHIPSET;
		else
			p->leds_on_screen &= ~STATUSLINE_CHIPSET;
		return 1;
	}
	if (cfgfile_yesno (option, value, _T("show_leds_rtg"), &vb)) {
		if (vb)
			p->leds_on_screen |= STATUSLINE_RTG;
		else
			p->leds_on_screen &= ~STATUSLINE_RTG;
		return 1;
	}
	if (_tcscmp (option, _T("show_leds_enabled")) == 0 || _tcscmp (option, _T("show_leds_enabled_rtg")) == 0) {
		TCHAR tmp[MAX_DPATH];
		int idx = _tcscmp (option, _T("show_leds_enabled")) == 0 ? 0 : 1;
		p->leds_on_screen_mask[idx] = 0;
		_tcscpy (tmp, value);
		_tcscat (tmp, _T(","));
		TCHAR *s = tmp;
		for (;;) {
			TCHAR *s2 = s;
			TCHAR *s3 = _tcschr (s, ':');
			s = _tcschr (s, ',');
			if (!s)
				break;
			if (s3 && s3 < s)
				s = s3;
			*s = 0;
			for (int i = 0; leds[i]; i++) {
				if (!_tcsicmp (s2, leds[i])) {
					p->leds_on_screen_mask[idx] |= 1 << i;
				}
			}
			s++;
		}
		return 1;
	}

	if (!_tcscmp (option, _T("osd_position"))) {
		TCHAR *s = value;
		p->osd_pos.x = 0;
		p->osd_pos.y = 0;
		while (s) {
			if (!_tcschr (s, ':'))
				break;
			p->osd_pos.x =  (int)(_tstof (s) * 10.0);
			s = _tcschr (s, ':');
			if (!s)
				break;
			if (s[-1] == '%')
				p->osd_pos.x += 30000;
			s++;
			p->osd_pos.y = (int)(_tstof (s) * 10.0);
			s += _tcslen (s);
			if (s[-1] == '%')
				p->osd_pos.y += 30000;
			break;
		}
		return 1;
	}

#ifdef GFXFILTER
	for (int j = 0; j < 2; j++) {
		struct gfx_filterdata *gf = &p->gf[j];
		if ((j == 0 && _tcscmp (option, _T("gfx_filter_overlay")) == 0) || (j == 1 && _tcscmp (option, _T("gfx_filter_overlay_rtg")) == 0)) {
			TCHAR *s = _tcschr (value, ',');
			gf->gfx_filteroverlay_overscan = 0;
			gf->gfx_filteroverlay_pos.x = 0;
			gf->gfx_filteroverlay_pos.y = 0;
			gf->gfx_filteroverlay_pos.width = 0;
			gf->gfx_filteroverlay_pos.height = 0;
			if (s)
				*s = 0;
			while (s) {
				*s++ = 0;
				gf->gfx_filteroverlay_overscan = _tstol (s);
				s = _tcschr (s, ':');
				if (!s)
					break;
				break;
			}
			_tcsncpy (gf->gfx_filteroverlay, value, sizeof gf->gfx_filteroverlay / sizeof (TCHAR) - 1);
			gf->gfx_filteroverlay[sizeof gf->gfx_filteroverlay / sizeof (TCHAR) - 1] = 0;
			return 1;
		}

		if ((j == 0 && (_tcscmp (option, _T("gfx_filtermask_pre")) == 0 || _tcscmp (option, _T("gfx_filtermask_post")) == 0)) ||
			(j == 1 && (_tcscmp (option, _T("gfx_filtermask_pre_rtg")) == 0 || _tcscmp (option, _T("gfx_filtermask_post_rtg")) == 0))) {
			if (_tcscmp (option, _T("gfx_filtermask_pre")) == 0 || _tcscmp (option, _T("gfx_filtermask_pre_rtg")) == 0) {
				for (int i = 0; i < MAX_FILTERSHADERS; i++) {
					if (gf->gfx_filtermask[i][0] == 0) {
						_tcscpy (gf->gfx_filtermask[i], value);
						break;
					}
				}
			} else {
				for (int i = 0; i < MAX_FILTERSHADERS; i++) {
					if (gf->gfx_filtermask[i + MAX_FILTERSHADERS][0] == 0) {
						_tcscpy (gf->gfx_filtermask[i + MAX_FILTERSHADERS], value);
						break;
					}
				}
			}
			return 1;
		}

		if ((j == 0 && (_tcscmp (option, _T("gfx_filter_pre")) == 0 || _tcscmp (option, _T("gfx_filter_post")) == 0)) ||
			(j == 1 && (_tcscmp (option, _T("gfx_filter_pre_rtg")) == 0 || _tcscmp (option, _T("gfx_filter_post_rtg")) == 0))) {
			TCHAR *s = _tcschr (value, ':');
			if (s) {
				*s++ = 0;
				if (!_tcscmp (value, _T("D3D"))) {
					p->gfx_api = 1;
					if (_tcscmp (option, _T("gfx_filter_pre")) == 0 || _tcscmp (option, _T("gfx_filter_pre_rtg")) == 0) {
						for (int i = 0; i < MAX_FILTERSHADERS; i++) {
							if (gf->gfx_filtershader[i][0] == 0) {
								_tcscpy (gf->gfx_filtershader[i], s);
								break;
							}
						}
					} else {
						for (int i = 0; i < MAX_FILTERSHADERS; i++) {
							if (gf->gfx_filtershader[i + MAX_FILTERSHADERS][0] == 0) {
								_tcscpy (gf->gfx_filtershader[i + MAX_FILTERSHADERS], s);
								break;
							}
						}
					}
				}
			}
			return 1;
		}

		if ((j == 0 && _tcscmp (option, _T("gfx_filter")) == 0) || (j == 1 && _tcscmp (option, _T("gfx_filter_rtg")) == 0)) {
			TCHAR *s = _tcschr (value, ':');
			gf->gfx_filter = 0;
			if (s) {
				*s++ = 0;
				if (!_tcscmp (value, _T("D3D"))) {
					p->gfx_api = 1;
					_tcscpy (gf->gfx_filtershader[2 * MAX_FILTERSHADERS], s);
					for (int i = 0; i < 2 * MAX_FILTERSHADERS; i++) {
						if (!_tcsicmp (gf->gfx_filtershader[i], s)) {
							gf->gfx_filtershader[i][0] = 0;
							gf->gfx_filtermask[i][0] = 0;
						}
					}
				}
			}
			if (!_tcscmp (value, _T("direct3d"))) {
				p->gfx_api = 1; // forwards compatibiity
			} else {
				int i = 0;
				while(uaefilters[i].name) {
					if (!_tcscmp (uaefilters[i].cfgname, value)) {
						gf->gfx_filter = uaefilters[i].type;
						break;
					}
					i++;
				}
			}
			return 1;
		}
		if (j == 0 && _tcscmp (option, _T("gfx_filter_mode")) == 0) {
			cfgfile_strval (option, value, _T("gfx_filter_mode"), &gf->gfx_filter_filtermode, filtermode2, 0);
			return 1;
		}
		if (j == 1 && _tcscmp (option, _T("gfx_filter_mode_rtg")) == 0) {
			cfgfile_strval (option, value, _T("gfx_filter_mode_rtg"), &gf->gfx_filter_filtermode, filtermode2, 0);
			return 1;
		}

		if ((j == 0 && cfgfile_string (option, value, _T("gfx_filter_aspect_ratio"), tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) ||
			(j == 1 && cfgfile_string (option, value, _T("gfx_filter_aspect_ratio_rtg"), tmpbuf, sizeof tmpbuf / sizeof (TCHAR)))) {
			int v1, v2;
			TCHAR *s;

			gf->gfx_filter_aspect = -1;
			v1 = _tstol (tmpbuf);
			s = _tcschr (tmpbuf, ':');
			if (s) {
				v2 = _tstol (s + 1);
				if (v1 < 0 || v2 < 0)
					gf->gfx_filter_aspect = -1;
				else if (v1 == 0 || v2 == 0)
					gf->gfx_filter_aspect = 0;
				else
					gf->gfx_filter_aspect = v1 * ASPECTMULT + v2;
			}
			return 1;
		}
	}
#endif

	if (_tcscmp (option, _T("gfx_width")) == 0 || _tcscmp (option, _T("gfx_height")) == 0) {
		cfgfile_intval (option, value, _T("gfx_width"), &p->gfx_size_win.width, 1);
		cfgfile_intval (option, value, _T("gfx_height"), &p->gfx_size_win.height, 1);
		p->gfx_size_fs.width = p->gfx_size_win.width;
		p->gfx_size_fs.height = p->gfx_size_win.height;
		return 1;
	}

	if (_tcscmp (option, _T("gfx_fullscreen_multi")) == 0 || _tcscmp (option, _T("gfx_windowed_multi")) == 0) {
		TCHAR tmp[256], *tmpp, *tmpp2;
		struct wh *wh = p->gfx_size_win_xtra;
		if (_tcscmp (option, _T("gfx_fullscreen_multi")) == 0)
			wh = p->gfx_size_fs_xtra;
		_stprintf (tmp, _T(",%s,"), value);
		tmpp2 = tmp;
		for (i = 0; i < 4; i++) {
			tmpp = _tcschr (tmpp2, ',');
			tmpp++;
			wh[i].width = _tstol (tmpp);
			while (*tmpp != ',' && *tmpp != 'x' && *tmpp != '*')
				tmpp++;
			wh[i].height = _tstol (tmpp + 1);
			tmpp2 = tmpp;
		}
		return 1;
	}

	if (_tcscmp (option, _T("joyportfriendlyname0")) == 0 || _tcscmp (option, _T("joyportfriendlyname1")) == 0) {
		inputdevice_joyport_config (p, value, _tcscmp (option, _T("joyportfriendlyname0")) == 0 ? 0 : 1, -1, 2, true);
		return 1;
	}
	if (_tcscmp (option, _T("joyportfriendlyname2")) == 0 || _tcscmp (option, _T("joyportfriendlyname3")) == 0) {
		inputdevice_joyport_config (p, value, _tcscmp (option, _T("joyportfriendlyname2")) == 0 ? 2 : 3, -1, 2, true);
		return 1;
	}
	if (_tcscmp (option, _T("joyportname0")) == 0 || _tcscmp (option, _T("joyportname1")) == 0) {
		inputdevice_joyport_config (p, value, _tcscmp (option, _T("joyportname0")) == 0 ? 0 : 1, -1, 1, true);
		return 1;
	}
	if (_tcscmp (option, _T("joyportname2")) == 0 || _tcscmp (option, _T("joyportname3")) == 0) {
		inputdevice_joyport_config (p, value, _tcscmp (option, _T("joyportname2")) == 0 ? 2 : 3, -1, 1, true);
		return 1;
	}
	if (_tcscmp (option, _T("joyport0")) == 0 || _tcscmp (option, _T("joyport1")) == 0) {
		inputdevice_joyport_config (p, value, _tcscmp (option, _T("joyport0")) == 0 ? 0 : 1, -1, 0, true);
		return 1;
	}
	if (_tcscmp (option, _T("joyport2")) == 0 || _tcscmp (option, _T("joyport3")) == 0) {
		inputdevice_joyport_config (p, value, _tcscmp (option, _T("joyport2")) == 0 ? 2 : 3, -1, 0, true);
		return 1;
	}
	if (cfgfile_strval (option, value, _T("joyport0mode"), &p->jports[0].mode, joyportmodes, 0))
		return 1;
	if (cfgfile_strval (option, value, _T("joyport1mode"), &p->jports[1].mode, joyportmodes, 0))
		return 1;
	if (cfgfile_strval (option, value, _T("joyport2mode"), &p->jports[2].mode, joyportmodes, 0))
		return 1;
	if (cfgfile_strval (option, value, _T("joyport3mode"), &p->jports[3].mode, joyportmodes, 0))
		return 1;
	if (cfgfile_strval (option, value, _T("joyport0autofire"), &p->jports[0].autofire, joyaf, 0))
		return 1;
	if (cfgfile_strval (option, value, _T("joyport1autofire"), &p->jports[1].autofire, joyaf, 0))
		return 1;
	if (cfgfile_strval (option, value, _T("joyport2autofire"), &p->jports[2].autofire, joyaf, 0))
		return 1;
	if (cfgfile_strval (option, value, _T("joyport3autofire"), &p->jports[3].autofire, joyaf, 0))
		return 1;
	if (cfgfile_yesno (option, value, _T("joyport0keyboardoverride"), &vb)) {
		p->jports[0].nokeyboardoverride = !vb;
		return 1;
	}
	if (cfgfile_yesno (option, value, _T("joyport1keyboardoverride"), &vb)) {
		p->jports[1].nokeyboardoverride = !vb;
		return 1;
	}
	if (cfgfile_yesno (option, value, _T("joyport2keyboardoverride"), &vb)) {
		p->jports[2].nokeyboardoverride = !vb;
		return 1;
	}
	if (cfgfile_yesno (option, value, _T("joyport3keyboardoverride"), &vb)) {
		p->jports[3].nokeyboardoverride = !vb;
		return 1;
	}

	if (cfgfile_path (option, value, _T("statefile_quit"), p->quitstatefile, sizeof p->quitstatefile / sizeof (TCHAR)))
		return 1;

	if (cfgfile_string (option, value, _T("statefile_name"), tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) {
		fetch_statefilepath (savestate_fname, sizeof savestate_fname / sizeof (TCHAR));
		_tcscat (savestate_fname, tmpbuf);
		if (_tcslen (savestate_fname) >= 4 && _tcsicmp (savestate_fname + _tcslen (savestate_fname) - 4, _T(".uss")))
			_tcscat (savestate_fname, _T(".uss"));
		return 1;
	}

	if (cfgfile_path (option, value, _T("statefile"), tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) {
		_tcscpy (p->statefile, tmpbuf);
		_tcscpy (savestate_fname, tmpbuf);
		if (zfile_exists (savestate_fname)) {
			savestate_state = STATE_DORESTORE;
		} else {
			int ok = 0;
			if (savestate_fname[0]) {
				for (;;) {
					TCHAR *p;
					if (my_existsdir (savestate_fname)) {
						ok = 1;
						break;
					}
					p = _tcsrchr (savestate_fname, '\\');
					if (!p)
						p = _tcsrchr (savestate_fname, '/');
					if (!p)
						break;
					*p = 0;
				}
			}
			if (!ok) {
				TCHAR tmp[MAX_DPATH];
				fetch_statefilepath (tmp, sizeof tmp / sizeof (TCHAR));
				_tcscat (tmp, savestate_fname);
				if (zfile_exists (tmp)) {
					_tcscpy (savestate_fname, tmp);
					savestate_state = STATE_DORESTORE;
				} else {
					savestate_fname[0] = 0;
				}
			}
		}
		return 1;
	}

	if (cfgfile_strval (option, value, _T("sound_channels"), &p->sound_stereo, stereomode, 1)) {
		if (p->sound_stereo == SND_NONE) { /* "mixed stereo" compatibility hack */
			p->sound_stereo = SND_STEREO;
			p->sound_mixed_stereo_delay = 5;
			p->sound_stereo_separation = 7;
		}
		return 1;
	}

	if (_tcscmp (option, _T("kbd_lang")) == 0) {
		KbdLang l;
		if ((l = KBD_LANG_DE, strcasecmp (value, _T("de")) == 0)
			|| (l = KBD_LANG_DK, strcasecmp (value, _T("dk")) == 0)
			|| (l = KBD_LANG_SE, strcasecmp (value, _T("se")) == 0)
			|| (l = KBD_LANG_US, strcasecmp (value, _T("us")) == 0)
			|| (l = KBD_LANG_FR, strcasecmp (value, _T("fr")) == 0)
			|| (l = KBD_LANG_IT, strcasecmp (value, _T("it")) == 0)
			|| (l = KBD_LANG_ES, strcasecmp (value, _T("es")) == 0))
			p->keyboard_lang = l;
		else
			write_log (_T("Unknown keyboard language\n"));
		return 1;
	}

	if (cfgfile_string (option, value, _T("config_version"), tmpbuf, sizeof (tmpbuf) / sizeof (TCHAR))) {
		TCHAR *tmpp2;
		tmpp = _tcschr (value, '.');
		if (tmpp) {
			*tmpp++ = 0;
			tmpp2 = tmpp;
			p->config_version = _tstol (tmpbuf) << 16;
			tmpp = _tcschr (tmpp, '.');
			if (tmpp) {
				*tmpp++ = 0;
				p->config_version |= _tstol (tmpp2) << 8;
				p->config_version |= _tstol (tmpp);
			}
		}
		return 1;
	}

	if (cfgfile_string (option, value, _T("keyboard_leds"), tmpbuf, sizeof (tmpbuf) / sizeof (TCHAR))) {
		TCHAR *tmpp2 = tmpbuf;
		int i, num;
		p->keyboard_leds[0] = p->keyboard_leds[1] = p->keyboard_leds[2] = 0;
		p->keyboard_leds_in_use = 0;
		_tcscat (tmpbuf, _T(","));
		for (i = 0; i < 3; i++) {
			tmpp = _tcschr (tmpp2, ':');
			if (!tmpp)
				break;
			*tmpp++= 0;
			num = -1;
			if (!strcasecmp (tmpp2, _T("numlock")))
				num = 0;
			if (!strcasecmp (tmpp2, _T("capslock")))
				num = 1;
			if (!strcasecmp (tmpp2, _T("scrolllock")))
				num = 2;
			tmpp2 = tmpp;
			tmpp = _tcschr (tmpp2, ',');
			if (!tmpp)
				break;
			*tmpp++= 0;
			if (num >= 0) {
				p->keyboard_leds[num] = match_string (kbleds, tmpp2);
				if (p->keyboard_leds[num])
					p->keyboard_leds_in_use = 1;
			}
			tmpp2 = tmpp;
		}
		return 1;
	}

	if (_tcscmp (option, _T("displaydata")) == 0 || _tcscmp (option, _T("displaydata_pal")) == 0 || _tcscmp (option, _T("displaydata_ntsc")) == 0) {
		_tcsncpy (tmpbuf, value, sizeof tmpbuf / sizeof (TCHAR) - 1);
		tmpbuf[sizeof tmpbuf / sizeof (TCHAR) - 1] = '\0';

		int vert = -1, horiz = -1, lace = -1, ntsc = -1, framelength = -1, vsync = -1;
		bool locked = false;
		bool rtg = false;
		double rate = -1;
		TCHAR cmd[MAX_DPATH], label[16] = { 0 };
		TCHAR *tmpp = tmpbuf;
		TCHAR *end = tmpbuf + _tcslen (tmpbuf);
		cmd[0] = 0;
		for (;;) {
			TCHAR *next = _tcschr (tmpp, ',');
			TCHAR *equals = _tcschr (tmpp, '=');

			if (!next)
				next = end;
			if (equals == NULL || equals > next)
				equals = NULL;
			else
				equals++;
			*next = 0;

			if (rate < 0)
				rate = _tstof (tmpp);
			else if (!_tcsnicmp (tmpp, _T("v="), 2))
				vert = _tstol (equals);
			else if (!_tcsnicmp (tmpp, _T("h="), 2))
				horiz = _tstol (equals);
			else if (!_tcsnicmp (tmpp, _T("t="), 2))
				_tcsncpy (label, equals, sizeof label / sizeof (TCHAR) - 1);
			else if (equals) {
				if (_tcslen (cmd) + _tcslen (tmpp) + 2 < sizeof (cmd) / sizeof(TCHAR)) {
					_tcscat (cmd, tmpp);
					_tcscat (cmd, _T("\n"));
				}
			}
			if (!_tcsnicmp (tmpp, _T("locked"), 4))
				locked = true;
			if (!_tcsnicmp (tmpp, _T("nlace"), 5))
				lace = 0;
			if (!_tcsnicmp (tmpp, _T("lace"), 4))
				lace = 1;
			if (!_tcsnicmp (tmpp, _T("nvsync"), 5))
				vsync = 0;
			if (!_tcsnicmp (tmpp, _T("vsync"), 4))
				vsync = 1;
			if (!_tcsnicmp (tmpp, _T("ntsc"), 4))
				ntsc = 1;
			if (!_tcsnicmp (tmpp, _T("pal"), 3))
				ntsc = 0;
			if (!_tcsnicmp (tmpp, _T("lof"), 3))
				framelength = 1;
			if (!_tcsnicmp (tmpp, _T("shf"), 3))
				framelength = 0;
			if (!_tcsnicmp (tmpp, _T("rtg"), 3))
				rtg = true;
			tmpp = next;
			if (tmpp >= end)
				break;
			tmpp++;
		}
		if (rate > 0) {
			for (int i = 0; i < MAX_CHIPSET_REFRESH; i++) {
				if (_tcscmp (option, _T("displaydata_pal")) == 0) {
					i = CHIPSET_REFRESH_PAL;
					p->cr[i].rate = -1;
					_tcscpy (label, _T("PAL"));
				} else if (_tcscmp (option, _T("displaydata_ntsc")) == 0) {
					i = CHIPSET_REFRESH_NTSC;
					p->cr[i].rate = -1;
					_tcscpy (label, _T("NTSC"));
				}
				if (p->cr[i].rate <= 0) {
					p->cr[i].horiz = horiz;
					p->cr[i].vert = vert;
					p->cr[i].lace = lace;
					p->cr[i].ntsc = ntsc;
					p->cr[i].vsync = vsync;
					p->cr[i].locked = locked;
					p->cr[i].rtg = rtg;
					p->cr[i].framelength = framelength;
					p->cr[i].rate = rate;
					_tcscpy (p->cr[i].commands, cmd);
					_tcscpy (p->cr[i].label, label);
					break;
				}
			}
		}
		return 1;
	}

#ifdef WITH_SLIRP
	if (cfgfile_string (option, value, _T("slirp_ports"), tmpbuf, sizeof (tmpbuf) / sizeof TCHAR)) {
		TCHAR *tmpp2 = tmpbuf;
		_tcscat (tmpbuf, _T(","));
		for (;;) {
			tmpp = _tcschr (tmpp2, ',');
			if (!tmpp)
				break;
			*tmpp++= 0;
			for (i = 0; i < MAX_SLIRP_REDIRS; i++) {
				struct slirp_redir *sr = &p->slirp_redirs[i];
				if (sr->proto == 0) {
					sr->dstport = _tstol (tmpp2);
					sr->proto = 1;
					break;
				}
			}
			tmpp2 = tmpp;
		}
		return 1;
	}
	if (cfgfile_string (option, value, _T("slirp_redir"), tmpbuf, sizeof (tmpbuf) / sizeof TCHAR)) {
		TCHAR *tmpp2 = tmpbuf;
		_tcscat (tmpbuf, _T(":"));
		for (i = 0; i < MAX_SLIRP_REDIRS; i++) {
			struct slirp_redir *sr = &p->slirp_redirs[i];
			if (sr->proto == 0) {
				char *s;
				tmpp = _tcschr (tmpp2, ':');
				if (!tmpp)
					break;
				*tmpp++= 0;
				if (!_tcsicmp (tmpp2, _T("tcp")))
					sr->proto = 1;
				else if (!_tcsicmp (tmpp2, _T("udp")))
					sr->proto = 2;
				else
					break;
				tmpp2 = tmpp;
				tmpp = _tcschr (tmpp2, ':');
				if (!tmpp) {
					sr->proto = 0;
					break;
				}
				*tmpp++= 0;
				sr->dstport = _tstol (tmpp2);
				tmpp2 = tmpp;
				tmpp = _tcschr (tmpp2, ':');
				if (!tmpp) {
					sr->proto = 0;
					break;
				}
				*tmpp++= 0;
				sr->srcport = _tstol (tmpp2);
				tmpp2 = tmpp;
				tmpp = _tcschr (tmpp2, ':');
				if (!tmpp)
					break;
				*tmpp++= 0;
				s = ua (tmpp2);
				sr->addr = inet_addr (s);
				xfree (s);
			}
		}
		return 1;
	}
#endif

	return 0;
}

static void decode_rom_ident (TCHAR *romfile, int maxlen, const TCHAR *ident, int romflags)
{
	const TCHAR *p;
	int ver, rev, subver, subrev, round, i;
	TCHAR model[64], *modelp;
	struct romlist **rl;
	TCHAR *romtxt;

	if (!ident[0])
		return;
	romtxt = xmalloc (TCHAR, 10000);
	romtxt[0] = 0;
	for (round = 0; round < 2; round++) {
		ver = rev = subver = subrev = -1;
		modelp = NULL;
		memset (model, 0, sizeof model);
		p = ident;
		while (*p) {
			TCHAR c = *p++;
			int *pp1 = NULL, *pp2 = NULL;
			if (_totupper (c) == 'V' && _istdigit (*p)) {
				pp1 = &ver;
				pp2 = &rev;
			} else if (_totupper (c) == 'R' && _istdigit (*p)) {
				pp1 = &subver;
				pp2 = &subrev;
			} else if (!_istdigit (c) && c != ' ') {
				_tcsncpy (model, p - 1, (sizeof model) / sizeof (TCHAR) - 1);
				p += _tcslen (model);
				modelp = model;
			}
			if (pp1) {
				*pp1 = _tstol (p);
				while (*p != 0 && *p != '.' && *p != ' ')
					p++;
				if (*p == '.') {
					p++;
					if (pp2)
						*pp2 = _tstol (p);
				}
			}
			if (*p == 0 || *p == ';') {
				rl = getromlistbyident (ver, rev, subver, subrev, modelp, romflags, round > 0);
				if (rl) {
					for (i = 0; rl[i]; i++) {
						if (round) {
							TCHAR romname[MAX_DPATH];
							getromname(rl[i]->rd, romname);
							_tcscat (romtxt, romname);
							_tcscat (romtxt, _T("\n"));
						} else {
							_tcsncpy (romfile, rl[i]->path, maxlen);
							goto end;
						}
					}
					xfree (rl);
				}
			}
		}
	}
end:
	if (round && romtxt[0]) {
		notify_user_parms (NUMSG_ROMNEED, romtxt, romtxt);
	}
	xfree (romtxt);
}

static struct uaedev_config_data *getuci (struct uae_prefs *p)
{
	if (p->mountitems < MOUNT_CONFIG_SIZE)
		return &p->mountconfig[p->mountitems++];
	return NULL;
}

struct uaedev_config_data *add_filesys_config (struct uae_prefs *p, int index, struct uaedev_config_info *ci)
{
	struct uaedev_config_data *uci;
	int i;

	if (index < 0 && (ci->type == UAEDEV_DIR || ci->type == UAEDEV_HDF) && ci->devname && _tcslen (ci->devname) > 0) {
		for (i = 0; i < p->mountitems; i++) {
			if (p->mountconfig[i].ci.devname && !_tcscmp (p->mountconfig[i].ci.devname, ci->devname))
				return NULL;
		}
	}
	for (;;) {
		if (ci->type == UAEDEV_CD) {
			if (ci->controller_type >= HD_CONTROLLER_TYPE_IDE_FIRST && ci->controller_type <= HD_CONTROLLER_TYPE_IDE_LAST)
				break;
			if (ci->controller_type >= HD_CONTROLLER_TYPE_SCSI_FIRST && ci->controller_type <= HD_CONTROLLER_TYPE_SCSI_LAST)
				break;
		} else if (ci->type == UAEDEV_TAPE) {
			if (ci->controller_type == HD_CONTROLLER_TYPE_UAE)
				break;
			if (ci->controller_type >= HD_CONTROLLER_TYPE_SCSI_FIRST && ci->controller_type <= HD_CONTROLLER_TYPE_SCSI_LAST)
				break;
		} else {
			break;
		}
		return NULL;
	}

	if (index < 0) {
		if (ci->controller_type != HD_CONTROLLER_TYPE_UAE) {
			int ctrl = ci->controller_type;
			int ctrlunit = ci->controller_type_unit;
			int cunit = ci->controller_unit;
			for (;;) {
				for (i = 0; i < p->mountitems; i++) {
					if (p->mountconfig[i].ci.controller_type == ctrl && p->mountconfig[i].ci.controller_type_unit == ctrlunit && p->mountconfig[i].ci.controller_unit == cunit) {
						cunit++;
						if (ctrl >= HD_CONTROLLER_TYPE_IDE_FIRST && ctrl <= HD_CONTROLLER_TYPE_IDE_LAST && cunit == 4)
							return NULL;
						if (ctrl >= HD_CONTROLLER_TYPE_SCSI_FIRST && ctrl <= HD_CONTROLLER_TYPE_SCSI_LAST && cunit == 7)
							return NULL;
					}
				}
				if (i == p->mountitems) {
					ci->controller_unit = cunit;
					break;
				}
			}
		}
		if (ci->type == UAEDEV_CD) {
			for (i = 0; i < p->mountitems; i++) {
				if (p->mountconfig[i].ci.type == ci->type)
					return NULL;
			}
		}
		uci = getuci (p);
		uci->configoffset = -1;
		uci->unitnum = -1;
	} else {
		uci = &p->mountconfig[index];
	}
	if (!uci)
		return NULL;

	memcpy (&uci->ci, ci, sizeof (struct uaedev_config_info));
	validatedevicename (uci->ci.devname, NULL);
	validatevolumename (uci->ci.volname, NULL);
	if (!uci->ci.devname[0] && ci->type != UAEDEV_CD && ci->type != UAEDEV_TAPE) {
		TCHAR base[32];
		TCHAR base2[32];
		int num = 0;
		if (uci->ci.rootdir[0] == 0 && ci->type == UAEDEV_DIR)
			_tcscpy (base, _T("RDH"));
		else
			_tcscpy (base, _T("DH"));
		_tcscpy (base2, base);
		for (i = 0; i < p->mountitems; i++) {
			_stprintf (base2, _T("%s%d"), base, num);
			if (!_tcsicmp(base2, p->mountconfig[i].ci.devname)) {
				num++;
				i = -1;
				continue;
			}
		}
		_tcscpy (uci->ci.devname, base2);
		validatedevicename (uci->ci.devname, NULL);
	}
	if (ci->type == UAEDEV_DIR) {
		TCHAR *s = filesys_createvolname (uci->ci.volname, uci->ci.rootdir, NULL, _T("Harddrive"));
		_tcscpy (uci->ci.volname, s);
		xfree (s);
	}
	return uci;
}

static void parse_addmem (struct uae_prefs *p, TCHAR *buf, int num)
{
	int size = 0, addr = 0;

	if (!getintval2 (&buf, &addr, ',', false))
		return;
	if (!getintval2 (&buf, &size, 0, true))
		return;
	if (addr & 0xffff)
		return;
	if ((size & 0xffff) || (size & 0xffff0000) == 0)
		return;
	p->custom_memory_addrs[num] = addr;
	p->custom_memory_sizes[num] = size;
}

static void get_filesys_controller (const TCHAR *hdc, int *type, int *typenum, int *num)
{
	int hdcv = HD_CONTROLLER_TYPE_UAE;
	int hdunit = 0;
	int idx = 0;
	if(_tcslen (hdc) >= 4 && !_tcsncmp (hdc, _T("ide"), 3)) {
		hdcv = HD_CONTROLLER_TYPE_IDE_AUTO;
		hdunit = hdc[3] - '0';
		if (hdunit < 0 || hdunit > 3)
			hdunit = 0;
	} else if(_tcslen (hdc) >= 5 && !_tcsncmp (hdc, _T("scsi"), 4)) {
		hdcv = HD_CONTROLLER_TYPE_SCSI_AUTO;
		hdunit = hdc[4] - '0';
		if (hdunit < 0 || hdunit > 7)
			hdunit = 0;
	}
	if (hdcv > HD_CONTROLLER_TYPE_UAE) {
		bool found = false;
		const TCHAR *ext = _tcsrchr (hdc, '_');
		if (ext) {
			ext++;
			int len = _tcslen(ext);
			if (len > 2 && ext[len - 2] == '-' && ext[len - 1] >= '2' && ext[len - 1] <= '9') {
				idx = ext[len - 1] - '1';
				len -= 2;
			}
			for (int i = 0; hdcontrollers[i]; i++) {
				const TCHAR *ext2 = _tcsrchr(hdcontrollers[i], '_');
				if (ext2) {
					ext2++;
					if (_tcslen(ext2) == len && !_tcsnicmp(ext, ext2, len) && hdc[0] == hdcontrollers[i][0]) {
						if (hdcv == HD_CONTROLLER_TYPE_IDE_AUTO) {
							hdcv = i;
						} else {
							hdcv = i + HD_CONTROLLER_EXPANSION_MAX;
						}
						found = true;
						break;
					}
				}
			}
			if (!found) {
				for (int i = 0; expansionroms[i].name; i++) {
					const struct expansionromtype *ert = &expansionroms[i];
					if (_tcslen(ert->name) == len && !_tcsnicmp(ext, ert->name, len)) {
						if (hdcv == HD_CONTROLLER_TYPE_IDE_AUTO) {
							hdcv = HD_CONTROLLER_TYPE_IDE_EXPANSION_FIRST + i;
						} else {
							hdcv = HD_CONTROLLER_TYPE_SCSI_EXPANSION_FIRST + i;
						}
						break;
					}
				}

			}
		}
	} else if (_tcslen (hdc) >= 6 && !_tcsncmp (hdc, _T("scsram"), 6)) {
		hdcv = HD_CONTROLLER_TYPE_PCMCIA_SRAM;
	} else if (_tcslen (hdc) >= 5 && !_tcsncmp (hdc, _T("scide"), 6)) {
		hdcv = HD_CONTROLLER_TYPE_PCMCIA_IDE;
	}
	if (idx >= MAX_DUPLICATE_EXPANSION_BOARDS)
		idx = MAX_DUPLICATE_EXPANSION_BOARDS - 1;
	*type = hdcv;
	*typenum = idx;
	*num = hdunit;
}

static bool parse_geo (const TCHAR *tname, struct uaedev_config_info *uci, struct hardfiledata *hfd, bool empty)
{
	struct zfile *f;
	int found;
	TCHAR buf[200];
	
	f = zfile_fopen (tname, _T("r"));
	if (!f)
		return false;
	found = hfd == NULL && !empty ? 2 : 0;
	if (found)
		write_log (_T("Geometry file '%s' detected\n"), tname);
	while (zfile_fgets (buf, sizeof buf / sizeof (TCHAR), f)) {
		int v;
		TCHAR *sep;
		
		my_trim (buf);
		if (_tcslen (buf) == 0)
			continue;
		if (buf[0] == '[' && buf[_tcslen (buf) - 1] == ']') {
			if (found > 1) {
				zfile_fclose (f);
				return true;
			}
			found = 0;
			buf[_tcslen (buf) - 1] = 0;
			my_trim (buf + 1);
			if (!_tcsicmp (buf + 1, _T("empty"))) {
				if (empty)
					found = 1;
			} else if (!_tcsicmp (buf + 1, _T("default"))) {
				if (!empty)
					found = 1;
			} else if (hfd) {
				uae_u64 size = _tstoi64 (buf + 1);
				if (size == hfd->virtsize)
					found = 2;
			}
			if (found)
				write_log (_T("Geometry file '%s', entry '%s' detected\n"), tname, buf + 1);
			continue;
		}
		if (!found)
			continue;

		sep =  _tcschr (buf, '=');
		if (!sep)
			continue;
		sep[0] = 0;

		TCHAR *key = my_strdup_trim (buf);
		TCHAR *val = my_strdup_trim (sep + 1);
		if (val[0] == '0' && _totupper (val[1]) == 'X') {
			TCHAR *endptr;
			v = _tcstol (val, &endptr, 16);
		} else {
			v = _tstol (val);
		}
		if (!_tcsicmp (key, _T("surfaces")))
			uci->surfaces = v;
		if (!_tcsicmp (key, _T("sectorspertrack")) || !_tcsicmp (key, _T("blockspertrack")))
			uci->sectors = v;
		if (!_tcsicmp (key, _T("sectorsperblock")))
			uci->sectorsperblock = v;
		if (!_tcsicmp (key, _T("reserved")))
			uci->reserved = v;
		if (!_tcsicmp (key, _T("lowcyl")))
			uci->lowcyl = v;
		if (!_tcsicmp (key, _T("highcyl")) || !_tcsicmp (key, _T("cyl")))
			uci->highcyl = v;
		if (!_tcsicmp (key, _T("blocksize")) || !_tcsicmp (key, _T("sectorsize")))
			uci->blocksize = v;
		if (!_tcsicmp (key, _T("buffers")))
			uci->buffers = v;
		if (!_tcsicmp (key, _T("maxtransfer")))
			uci->maxtransfer = v;
		if (!_tcsicmp (key, _T("interleave")))
			uci->interleave = v;
		if (!_tcsicmp (key, _T("dostype")))
			uci->dostype = v;
		if (!_tcsicmp (key, _T("bufmemtype")))
			uci->bufmemtype = v;
		if (!_tcsicmp (key, _T("stacksize")))
			uci->stacksize = v;
		if (!_tcsicmp (key, _T("mask")))
			uci->mask = v;
		if (!_tcsicmp (key, _T("unit")))
			uci->unit = v;
		if (!_tcsicmp (key, _T("controller")))
			get_filesys_controller (val, &uci->controller_type, &uci->controller_type_unit, &uci->controller_unit);
		if (!_tcsicmp (key, _T("flags")))
			uci->flags = v;
		if (!_tcsicmp (key, _T("priority")))
			uci->priority = v;
		if (!_tcsicmp (key, _T("forceload")))
			uci->forceload = v;
		if (!_tcsicmp (key, _T("bootpri"))) {
			if (v < -129)
				v = -129;
			if (v > 127)
				v = 127;
			uci->bootpri = v;
		}
		if (!_tcsicmp (key, _T("filesystem")))
			_tcscpy (uci->filesys, val);
		if (!_tcsicmp (key, _T("device")))
			_tcscpy (uci->devname, val);
		xfree (val);
		xfree (key);
	}
	zfile_fclose (f);
	return false;
}
bool get_hd_geometry (struct uaedev_config_info *uci)
{
	TCHAR tname[MAX_DPATH];

	fetch_configurationpath (tname, sizeof tname / sizeof (TCHAR));
	_tcscat (tname, _T("default.geo"));
	if (zfile_exists (tname)) {
		struct hardfiledata hfd;
		memset (&hfd, 0, sizeof hfd);
		hfd.ci.readonly = true;
		hfd.ci.blocksize = 512;
		if (hdf_open (&hfd, uci->rootdir) > 0) {
			parse_geo (tname, uci, &hfd, false);
			hdf_close (&hfd);
		} else {
			parse_geo (tname, uci, NULL, true);
		}
	}
	if (uci->rootdir[0]) {
		_tcscpy (tname, uci->rootdir);
		_tcscat (tname, _T(".geo"));
		return parse_geo (tname, uci, NULL, false);
	}
	return false;
}

static int cfgfile_parse_partial_newfilesys (struct uae_prefs *p, int nr, int type, const TCHAR *value, int unit, bool uaehfentry)
{
	TCHAR *tmpp;
	TCHAR *name = NULL, *path = NULL;

	// read only harddrive name
	if (!uaehfentry)
		return 0;
	if (type != 1)
		return 0;
	tmpp = getnextentry (&value, ',');
	if (!tmpp)
		return 0;
	xfree (tmpp);
	tmpp = getnextentry (&value, ':');
	if (!tmpp)
		return 0;
	xfree (tmpp);
	name = getnextentry (&value, ':');
	if (name && _tcslen (name) > 0) {
		path = getnextentry (&value, ',');
		if (path && _tcslen (path) > 0) {
			for (int i = 0; i < MAX_FILESYSTEM_UNITS; i++) {
				struct uaedev_config_info *uci = &p->mountconfig[i].ci;
				if (_tcsicmp (uci->rootdir, name) == 0) {
					_tcscat (uci->rootdir, _T(":"));
					_tcscat (uci->rootdir, path);
				}
			}
		}
	}
	xfree (path);
	xfree (name);
	return 1;
}

static int cfgfile_parse_newfilesys (struct uae_prefs *p, int nr, int type, TCHAR *value, int unit, bool uaehfentry)
{
	struct uaedev_config_info uci;
	TCHAR *tmpp = _tcschr (value, ','), *tmpp2;
	TCHAR *str = NULL;
	TCHAR devname[MAX_DPATH], volname[MAX_DPATH];

	devname[0] = volname[0] = 0;
	uci_set_defaults (&uci, false);

	config_newfilesystem = 1;
	if (tmpp == 0)
		goto invalid_fs;

	*tmpp++ = '\0';
	if (strcasecmp (value, _T("ro")) == 0)
		uci.readonly = true;
	else if (strcasecmp (value, _T("rw")) == 0)
		uci.readonly = false;
	else
		goto invalid_fs;

	value = tmpp;
	if (type == 0) {
		uci.type = UAEDEV_DIR;
		tmpp = _tcschr (value, ':');
		if (tmpp == 0)
			goto empty_fs;
		*tmpp++ = 0;
		_tcscpy (devname, value);
		tmpp2 = tmpp;
		tmpp = _tcschr (tmpp, ':');
		if (tmpp == 0)
			goto empty_fs;
		*tmpp++ = 0;
		_tcscpy (volname, tmpp2);
		tmpp2 = tmpp;
		tmpp = _tcschr (tmpp, ',');
		if (tmpp == 0)
			goto empty_fs;
		*tmpp++ = 0;
		_tcscpy (uci.rootdir, tmpp2);
		_tcscpy (uci.volname, volname);
		_tcscpy (uci.devname, devname);
		if (! getintval (&tmpp, &uci.bootpri, 0))
			goto empty_fs;
	} else if (type == 1 || ((type == 2 || type == 3) && uaehfentry)) {
		tmpp = _tcschr (value, ':');
		if (tmpp == 0)
			goto invalid_fs;
		*tmpp++ = '\0';
		_tcscpy (devname, value);
		tmpp2 = tmpp;
		tmpp = _tcschr (tmpp, ',');
		if (tmpp == 0)
			goto invalid_fs;
		*tmpp++ = 0;
		_tcscpy (uci.rootdir, tmpp2);
		if (uci.rootdir[0] != ':')
			get_hd_geometry (&uci);
		_tcscpy (uci.devname, devname);
		if (! getintval (&tmpp, &uci.sectors, ',')
			|| ! getintval (&tmpp, &uci.surfaces, ',')
			|| ! getintval (&tmpp, &uci.reserved, ',')
			|| ! getintval (&tmpp, &uci.blocksize, ','))
			goto invalid_fs;
		if (getintval2 (&tmpp, &uci.bootpri, ',', false)) {
			tmpp2 = tmpp;
			tmpp = _tcschr (tmpp, ',');
			if (tmpp != 0) {
				*tmpp++ = 0;
				_tcscpy (uci.filesys, tmpp2);
				TCHAR *tmpp2 = _tcschr (tmpp, ',');
				if (tmpp2)
					*tmpp2++ = 0;
				get_filesys_controller (tmpp, &uci.controller_type, &uci.controller_type_unit, &uci.controller_unit);
				if (tmpp2) {
					if (getintval2 (&tmpp2, &uci.highcyl, ',', false)) {
						getintval (&tmpp2, &uci.pcyls, '/');
						getintval (&tmpp2, &uci.pheads, '/');
						getintval2 (&tmpp2, &uci.psecs, '/', true);
						if (uci.pheads && uci.psecs) {
							uci.physical_geometry = true;
						} else {
							uci.pheads = uci.psecs = uci.pcyls = 0;
							uci.physical_geometry = false;
						}
					}
				}
				uci.controller_media_type = 0;
				uci.unit_feature_level = 1;

				if (cfgfile_option_find(tmpp2, _T("CF")))
					uci.controller_media_type = 1;
				else if (cfgfile_option_find(tmpp2, _T("HD")))
					uci.controller_media_type = 0;

				if (cfgfile_option_find(tmpp2, _T("SCSI2")))
					uci.unit_feature_level = HD_LEVEL_SCSI_2;
				else if (cfgfile_option_find(tmpp2, _T("SCSI1")))
					uci.unit_feature_level = HD_LEVEL_SCSI_1;
				else if (cfgfile_option_find(tmpp2, _T("SASIE")))
					uci.unit_feature_level = HD_LEVEL_SASI_ENHANCED;
				else if (cfgfile_option_find(tmpp2, _T("SASI")))
					uci.unit_feature_level = HD_LEVEL_SASI;
				else if (cfgfile_option_find(tmpp2, _T("SASI_CHS")))
					uci.unit_feature_level = HD_LEVEL_OMTI;
				else if (cfgfile_option_find(tmpp2, _T("ATA2+S")))
					uci.unit_feature_level = HD_LEVEL_ATA_2S;
				else if (cfgfile_option_find(tmpp2, _T("ATA2+")))
					uci.unit_feature_level = HD_LEVEL_ATA_2;
				else if (cfgfile_option_find(tmpp2, _T("ATA1")))
					uci.unit_feature_level = HD_LEVEL_ATA_1;
			}
		}
		if (type == 2) {
			uci.device_emu_unit = unit;
			uci.blocksize = 2048;
			uci.readonly = true;
			uci.type = UAEDEV_CD;
		} else if (type == 3) {
			uci.device_emu_unit = unit;
			uci.blocksize = 512;
			uci.type = UAEDEV_TAPE;
		} else {
			uci.type = UAEDEV_HDF;
		}
	} else {
		goto invalid_fs;
	}
empty_fs:
	if (uci.rootdir[0]) {
		if (_tcslen (uci.rootdir) > 3 && uci.rootdir[0] == 'H' && uci.rootdir[1] == 'D' && uci.rootdir[2] == '_') {
			memmove (uci.rootdir, uci.rootdir + 2, (_tcslen (uci.rootdir + 2) + 1) * sizeof (TCHAR));
			uci.rootdir[0] = ':';
		}
		str = cfgfile_subst_path_load (UNEXPANDED, &p->path_hardfile, uci.rootdir, false);
		_tcscpy (uci.rootdir, str);
	}
#ifdef FILESYS
	add_filesys_config (p, nr, &uci);
#endif
	xfree (str);
	return 1;

invalid_fs:
	write_log (_T("Invalid filesystem/hardfile/cd specification.\n"));
	return 1;
}

static int cfgfile_parse_filesys (struct uae_prefs *p, const TCHAR *option, TCHAR *value)
{
	int i;

	for (i = 0; i < MAX_FILESYSTEM_UNITS; i++) {
		TCHAR tmp[100];
		_stprintf (tmp, _T("uaehf%d"), i);
		if (_tcscmp (option, tmp) == 0) {
			for (;;) {
				int  type = -1;
				int unit = -1;
				TCHAR *tmpp = _tcschr (value, ',');
				if (tmpp == NULL)
					return 1;
				*tmpp++ = 0;
				if (_tcsicmp (value, _T("hdf")) == 0) {
					type = 1;
					cfgfile_parse_partial_newfilesys (p, -1, type, tmpp, unit, true);
					return 1;
				} else if (_tcsnicmp (value, _T("cd"), 2) == 0 && (value[2] == 0 || value[3] == 0)) {
					unit = 0;
					if (value[2] > 0)
						unit = value[2] - '0';
					if (unit >= 0 && unit <= MAX_TOTAL_SCSI_DEVICES) {
						type = 2;
					}
				} else if (_tcsnicmp (value, _T("tape"), 4) == 0 && (value[4] == 0 || value[5] == 0)) {
					unit = 0;
					if (value[4] > 0)
						unit = value[4] - '0';
					if (unit >= 0 && unit <= MAX_TOTAL_SCSI_DEVICES) {
						type = 3;
					}
				} else if (_tcsicmp (value, _T("dir")) != 0) {
					type = 0;
					return 1;  /* ignore for now */
				}
				if (type >= 0)
					cfgfile_parse_newfilesys (p, -1, type, tmpp, unit, true);
				return 1;
			}
			return 1;
		} else if (!_tcsncmp (option, tmp, _tcslen (tmp)) && option[_tcslen (tmp)] == '_') {
			struct uaedev_config_info *uci = &currprefs.mountconfig[i].ci;
			if (uci->devname) {
				const TCHAR *s = &option[_tcslen (tmp) + 1];
				if (!_tcscmp (s, _T("bootpri"))) {
					getintval (&value, &uci->bootpri, 0);
				} else if (!_tcscmp (s, _T("read-only"))) {
					cfgfile_yesno (NULL, value, NULL, &uci->readonly);
				} else if (!_tcscmp (s, _T("volumename"))) {
					_tcscpy (uci->volname, value);
				} else if (!_tcscmp (s, _T("devicename"))) {
					_tcscpy (uci->devname, value);
				} else if (!_tcscmp (s, _T("root"))) {
					_tcscpy (uci->rootdir, value);
				} else if (!_tcscmp (s, _T("filesys"))) {
					_tcscpy (uci->filesys, value);
				} else if (!_tcscmp (s, _T("controller"))) {
					get_filesys_controller (value, &uci->controller_type, &uci->controller_type_unit, &uci->controller_unit);
				}
			}
		}
	}

	if (_tcscmp (option, _T("filesystem")) == 0
		|| _tcscmp (option, _T("hardfile")) == 0)
	{
		struct uaedev_config_info uci;
		TCHAR *tmpp = _tcschr (value, ',');
		TCHAR *str;
		bool hdf;

		uci_set_defaults (&uci, false);

		if (config_newfilesystem)
			return 1;

		if (tmpp == 0)
			goto invalid_fs;

		*tmpp++ = '\0';
		if (_tcscmp (value, _T("1")) == 0 || strcasecmp (value, _T("ro")) == 0
			|| strcasecmp (value, _T("readonly")) == 0
			|| strcasecmp (value, _T("read-only")) == 0)
			uci.readonly = true;
		else if (_tcscmp (value, _T("0")) == 0 || strcasecmp (value, _T("rw")) == 0
			|| strcasecmp (value, _T("readwrite")) == 0
			|| strcasecmp (value, _T("read-write")) == 0)
			uci.readonly = false;
		else
			goto invalid_fs;

		value = tmpp;
		if (_tcscmp (option, _T("filesystem")) == 0) {
			hdf = false;
			tmpp = _tcschr (value, ':');
			if (tmpp == 0)
				goto invalid_fs;
			*tmpp++ = '\0';
			_tcscpy (uci.volname, value);
			_tcscpy (uci.rootdir, tmpp);
		} else {
			hdf = true;
			if (! getintval (&value, &uci.sectors, ',')
				|| ! getintval (&value, &uci.surfaces, ',')
				|| ! getintval (&value, &uci.reserved, ',')
				|| ! getintval (&value, &uci.blocksize, ','))
				goto invalid_fs;
			_tcscpy (uci.rootdir, value);
		}
		str = cfgfile_subst_path_load (UNEXPANDED, &p->path_hardfile, uci.rootdir, true);
#ifdef FILESYS
		uci.type = hdf ? UAEDEV_HDF : UAEDEV_DIR;
		add_filesys_config (p, -1, &uci);
#endif
		xfree (str);
		return 1;
invalid_fs:
		write_log (_T("Invalid filesystem/hardfile specification.\n"));
		return 1;

	}

	if (_tcscmp (option, _T("filesystem2")) == 0)
		return cfgfile_parse_newfilesys (p, -1, 0, value, -1, false);
	if (_tcscmp (option, _T("hardfile2")) == 0)
		return cfgfile_parse_newfilesys (p, -1, 1, value, -1, false);
	if (_tcscmp (option, _T("filesystem_extra")) == 0) {
		int idx = 0;
		TCHAR *s = value;
		_tcscat(s, _T(","));
		struct uaedev_config_info *ci = NULL;
		for (;;) {
			TCHAR *tmpp = _tcschr (s, ',');
			if (tmpp == NULL)
				return 1;
			*tmpp++ = 0;
			if (idx == 0) {
				for (i = 0; i < p->mountitems; i++) {
					if (p->mountconfig[i].ci.devname && !_tcscmp (p->mountconfig[i].ci.devname, s)) {
						ci = &p->mountconfig[i].ci;
						break;
					}
				}
				if (!ci || ci->type != UAEDEV_DIR)
					return 1;
			} else {
				bool b = true;
				TCHAR *tmpp2 = _tcschr(s, '=');
				if (tmpp2) {
					*tmpp2++ = 0;
					if (!strcasecmp(tmpp2, _T("false")))
						b = false;
				}
				if (!strcasecmp(s, _T("inject_icons"))) {
					ci->inject_icons = b;
				}
			}
			idx++;
			s = tmpp;
		}
	}

	return 0;
}

bool cfgfile_board_enabled(struct uae_prefs *p, int romtype, int devnum)
{
	int idx;
	if (romtype == ROMTYPE_CPUBOARD && currprefs.cpuboard_type)
		return true;
	struct boardromconfig *brc = get_device_rom(p, romtype, devnum, &idx);
	if (!brc)
		return false;
	return brc->roms[idx].romfile[0] != 0;
}

static bool cfgfile_read_board_rom(struct uae_prefs *p, const TCHAR *option, const TCHAR *value, struct multipath *mp)
{
	TCHAR buf[256], buf2[MAX_DPATH], buf3[MAX_DPATH];
	bool dummy;
	int val;
	const struct expansionromtype *ert;

	for (int i = 0; expansionroms[i].name; i++) {
		struct boardromconfig *brc; 
		int idx;
		ert = &expansionroms[i];

		for (int j = 0; j < MAX_DUPLICATE_EXPANSION_BOARDS; j++) {
			TCHAR name[256];

			if (j == 0)
				_tcscpy(name, ert->name);
			else
				_stprintf(name, _T("%s-%d"), ert->name, j + 1);

			_stprintf(buf, _T("scsi_%s"), name);
			if (cfgfile_yesno(option, value, buf, &dummy)) {
				return true;
			}

			_stprintf(buf, _T("%s_rom_file"), name);
			if (cfgfile_path(option, value, buf, buf2, MAX_DPATH / sizeof (TCHAR), mp)) {
				if (buf2[0]) {
					brc = get_device_rom_new(p, ert->romtype, j, &idx);
					_tcscpy(brc->roms[idx].romfile, buf2);
				}
				return true;
			}

			_stprintf(buf, _T("%s_rom_file_id"), name);
			buf2[0] = 0;
			if (cfgfile_rom (option, value, buf, buf2, MAX_DPATH / sizeof (TCHAR))) {
				if (buf2[0]) {
					brc = get_device_rom_new(p, ert->romtype, j, &idx);
					_tcscpy(brc->roms[idx].romfile, buf2);
				}
				return true;
			}

			_stprintf(buf, _T("%s_rom"), name);
			if (cfgfile_string (option, value, buf, buf2, sizeof buf2 / sizeof (TCHAR))) {
				if (buf2[0]) {
					decode_rom_ident (buf3, sizeof(buf3) / sizeof (TCHAR), buf2, ert->romtype);
					if (buf3[0]) {
						brc = get_device_rom_new(p, ert->romtype, j, &idx);
						_tcscpy(brc->roms[idx].romident, buf3);
					}
				}
				return true;
			}

			_stprintf(buf, _T("%s_rom_options"), name);
			if (cfgfile_string (option, value, buf, buf2, sizeof buf2 / sizeof (TCHAR))) {
				brc = get_device_rom(p, ert->romtype, j, &idx);
				if (brc) {
					if (cfgfile_option_bool(buf2, _T("autoboot_disabled")) == 1) {
						brc->roms[idx].autoboot_disabled = true;
					}
					if (ert->settings) {
						for (int k = 0; ert->settings[k].name; k++) {
							if (cfgfile_option_find(buf2, ert->settings[k].configname)) {
								brc->roms[idx].device_settings |= 1 << k;
							}
						}
					}
					if (ert->id_jumper) {
						TCHAR *p = cfgfile_option_get(buf2, _T("id"));
						if (p) {
							brc->roms[idx].device_id = _tstol(p);
						}
					}
					if (ert->subtypes) {
						const struct expansionsubromtype *srt = ert->subtypes;
						TCHAR tmp[MAX_DPATH], *p;
						p = tmp;
						*p = 0;
						while (srt->name) {
							_tcscpy(p, srt->configname);
							p += _tcslen(p) + 1;
							p[0] = 0;
							srt++;
						}
						int v = cfgfile_option_select(buf2, _T("subtype"), tmp);
						if (v >= 0)
							brc->roms[idx].subtype = v;
					}
				}
				return true;
			}
		}

		_stprintf(buf, _T("%s_mem_size"), ert->name);
		if (cfgfile_intval (option, value, buf, &val, 0x40000)) {
			if (val) {
				brc = get_device_rom_new(p, ert->romtype, 0, &idx);
				brc->roms[idx].board_ram_size = val;
			}
			return true;
		}
	}
	return false;
}

static int cfgfile_parse_hardware (struct uae_prefs *p, const TCHAR *option, TCHAR *value)
{
	int tmpval, dummyint, i;
	bool tmpbool, dummybool;
	TCHAR *section = 0;
	TCHAR tmpbuf[CONFIG_BLEN];

	if (cfgfile_yesno (option, value, _T("cpu_cycle_exact"), &p->cpu_cycle_exact)
		|| cfgfile_yesno (option, value, _T("blitter_cycle_exact"), &p->blitter_cycle_exact)) {
			if (p->cpu_model >= 68020 && p->cachesize > 0)
				p->cpu_cycle_exact = p->blitter_cycle_exact = 0;
			/* we don't want cycle-exact in 68020/40+JIT modes */
			return 1;
	}
	if (cfgfile_yesno (option, value, _T("cycle_exact"), &tmpbool)) {
		p->cpu_cycle_exact = p->blitter_cycle_exact = tmpbool;
		if (p->cpu_model >= 68020 && p->cachesize > 0)
			p->cpu_cycle_exact = p->blitter_cycle_exact = false;
		// if old version and CE and fastest possible: set to approximate
		if (p->cpu_cycle_exact && p->config_version < ((2 << 16) | (8 << 8) | (2 << 0)) && p->m68k_speed < 0)
			p->m68k_speed = 0;
		return 1;
	}

	if (cfgfile_string (option, value, _T("cpu_multiplier"), tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) {
		p->cpu_clock_multiplier = (int)(_tstof (tmpbuf) * 256.0);
		return 1;
	}


	if (cfgfile_yesno (option, value, _T("scsi_a3000"), &dummybool)) {
		if (dummybool)
			p->cs_mbdmac = 1;
		return 1;
	}
	if (cfgfile_yesno (option, value, _T("scsi_a4000t"), &dummybool)) {
		if (dummybool)
			p->cs_mbdmac = 2;
		return 1;
	}

	if (cfgfile_string(option, value, _T("a2065"), p->a2065name, sizeof p->a2065name / sizeof(TCHAR)))
		return 1;
	if (cfgfile_string(option, value, _T("ne2000_pci"), p->ne2000pciname, sizeof p->ne2000pciname / sizeof(TCHAR)))
		return 1;

	if (cfgfile_yesno (option, value, _T("immediate_blits"), &p->immediate_blits)
		|| cfgfile_yesno (option, value, _T("fpu_no_unimplemented"), &p->fpu_no_unimplemented)
		|| cfgfile_yesno (option, value, _T("cpu_no_unimplemented"), &p->int_no_unimplemented)
		|| cfgfile_yesno (option, value, _T("cd32cd"), &p->cs_cd32cd)
		|| cfgfile_yesno (option, value, _T("cd32c2p"), &p->cs_cd32c2p)
		|| cfgfile_yesno(option, value, _T("cd32nvram"), &p->cs_cd32nvram)
		|| cfgfile_yesno(option, value, _T("cd32fmv"), &p->cs_cd32fmv)
		|| cfgfile_yesno(option, value, _T("cdtvcd"), &p->cs_cdtvcd)
		|| cfgfile_yesno(option, value, _T("cdtv-cr"), &p->cs_cdtvcr)
		|| cfgfile_yesno (option, value, _T("cdtvram"), &p->cs_cdtvram)
		|| cfgfile_yesno (option, value, _T("a1000ram"), &p->cs_a1000ram)
		|| cfgfile_yesno (option, value, _T("pcmcia"), &p->cs_pcmcia)
		|| cfgfile_yesno (option, value, _T("scsi_cdtv"), &p->cs_cdtvscsi)
		|| cfgfile_yesno (option, value, _T("cia_overlay"), &p->cs_ciaoverlay)
		|| cfgfile_yesno (option, value, _T("bogomem_fast"), &p->cs_slowmemisfast)
		|| cfgfile_yesno (option, value, _T("ksmirror_e0"), &p->cs_ksmirror_e0)
		|| cfgfile_yesno (option, value, _T("ksmirror_a8"), &p->cs_ksmirror_a8)
		|| cfgfile_yesno (option, value, _T("resetwarning"), &p->cs_resetwarning)
		|| cfgfile_yesno (option, value, _T("cia_todbug"), &p->cs_ciatodbug)
		|| cfgfile_yesno (option, value, _T("denise_noehb"), &p->cs_denisenoehb)
		|| cfgfile_yesno (option, value, _T("ics_agnus"), &p->cs_dipagnus)
		|| cfgfile_yesno (option, value, _T("z3_autoconfig"), &p->cs_z3autoconfig)
		|| cfgfile_yesno (option, value, _T("1mchipjumper"), &p->cs_1mchipjumper)
		|| cfgfile_yesno (option, value, _T("agnus_bltbusybug"), &p->cs_agnusbltbusybug)
		|| cfgfile_yesno (option, value, _T("fastmem_autoconfig"), &p->fastmem_autoconfig)
		|| cfgfile_yesno (option, value, _T("gfxcard_hardware_vblank"), &p->rtg_hardwareinterrupt)
		|| cfgfile_yesno (option, value, _T("gfxcard_hardware_sprite"), &p->rtg_hardwaresprite)
		|| cfgfile_yesno (option, value, _T("synchronize_clock"), &p->tod_hack)

		|| cfgfile_yesno (option, value, _T("kickshifter"), &p->kickshifter)
		|| cfgfile_yesno (option, value, _T("ks_write_enabled"), &p->rom_readwrite)
		|| cfgfile_yesno (option, value, _T("ntsc"), &p->ntscmode)
		|| cfgfile_yesno (option, value, _T("sana2"), &p->sana2)
		|| cfgfile_yesno (option, value, _T("genlock"), &p->genlock)
		|| cfgfile_yesno (option, value, _T("cpu_compatible"), &p->cpu_compatible)
		|| cfgfile_yesno(option, value, _T("cpu_24bit_addressing"), &p->address_space_24)
		|| cfgfile_yesno(option, value, _T("cpu_reset_pause"), &p->reset_delay)
		|| cfgfile_yesno(option, value, _T("parallel_on_demand"), &p->parallel_demand)
		|| cfgfile_yesno (option, value, _T("parallel_postscript_emulation"), &p->parallel_postscript_emulation)
		|| cfgfile_yesno (option, value, _T("parallel_postscript_detection"), &p->parallel_postscript_detection)
		|| cfgfile_yesno (option, value, _T("serial_on_demand"), &p->serial_demand)
		|| cfgfile_yesno (option, value, _T("serial_hardware_ctsrts"), &p->serial_hwctsrts)
		|| cfgfile_yesno (option, value, _T("serial_direct"), &p->serial_direct)
		|| cfgfile_yesno (option, value, _T("fpu_strict"), &p->fpu_strict)
		|| cfgfile_yesno (option, value, _T("fpu_softfloat"), &p->fpu_softfloat)
		|| cfgfile_yesno (option, value, _T("comp_nf"), &p->compnf)
		|| cfgfile_yesno (option, value, _T("comp_constjump"), &p->comp_constjump)
		|| cfgfile_yesno (option, value, _T("comp_oldsegv"), &p->comp_oldsegv)
		|| cfgfile_yesno (option, value, _T("compforcesettings"), &dummybool)
		|| cfgfile_yesno (option, value, _T("compfpu"), &p->compfpu)
		|| cfgfile_yesno (option, value, _T("comp_midopt"), &p->comp_midopt)
		|| cfgfile_yesno (option, value, _T("comp_lowopt"), &p->comp_lowopt)
		|| cfgfile_yesno (option, value, _T("rtg_nocustom"), &p->picasso96_nocustom)
		|| cfgfile_yesno (option, value, _T("floppy_write_protect"), &p->floppy_read_only)
		|| cfgfile_yesno (option, value, _T("uae_hide_autoconfig"), &p->uae_hide_autoconfig)
		|| cfgfile_yesno(option, value, _T("toccata"), &p->sound_toccata)
		|| cfgfile_yesno(option, value, _T("es1370_pci"), &p->sound_es1370)
		|| cfgfile_yesno(option, value, _T("fm801_pci"), &p->sound_fm801)
		|| cfgfile_yesno (option, value, _T("toccata_mixer"), &p->sound_toccata_mixer)
		|| cfgfile_yesno (option, value, _T("uaeserial"), &p->uaeserial))
		return 1;

	if (cfgfile_intval (option, value, _T("cachesize"), &p->cachesize, 1)
		|| cfgfile_intval (option, value, _T("cd32nvram_size"), &p->cs_cd32nvram_size, 1024)
		|| cfgfile_intval (option, value, _T("chipset_hacks"), &p->cs_hacks, 1)
		|| cfgfile_intval (option, value, _T("serial_stopbits"), &p->serial_stopbits, 1)
		|| cfgfile_intval (option, value, _T("cpu060_revision"), &p->cpu060_revision, 1)
		|| cfgfile_intval (option, value, _T("fpu_revision"), &p->fpu_revision, 1)
		|| cfgfile_intval (option, value, _T("cdtvramcard"), &p->cs_cdtvcard, 1)
		|| cfgfile_intval (option, value, _T("fatgary"), &p->cs_fatgaryrev, 1)
		|| cfgfile_intval (option, value, _T("ramsey"), &p->cs_ramseyrev, 1)
		|| cfgfile_doubleval (option, value, _T("chipset_refreshrate"), &p->chipset_refreshrate)
		|| cfgfile_intval(option, value, _T("cpuboardmem1_size"), &p->cpuboardmem1_size, 0x100000)
		|| cfgfile_intval(option, value, _T("cpuboardmem2_size"), &p->cpuboardmem2_size, 0x100000)
		|| cfgfile_intval(option, value, _T("fastmem_size"), &p->fastmem_size, 0x100000)
		|| cfgfile_intval(option, value, _T("fastmem_size_k"), &p->fastmem_size, 1024)
		|| cfgfile_intval (option, value, _T("fastmem2_size"), &p->fastmem2_size, 0x100000)
		|| cfgfile_intval (option, value, _T("fastmem2_size_k"), &p->fastmem2_size, 1024)
		|| cfgfile_intval (option, value, _T("mem25bit_size"), &p->mem25bit_size, 0x100000)
		|| cfgfile_intval (option, value, _T("a3000mem_size"), &p->mbresmem_low_size, 0x100000)
		|| cfgfile_intval (option, value, _T("mbresmem_size"), &p->mbresmem_high_size, 0x100000)
		|| cfgfile_intval (option, value, _T("z3mem_size"), &p->z3fastmem_size, 0x100000)
		|| cfgfile_intval (option, value, _T("z3mem2_size"), &p->z3fastmem2_size, 0x100000)
		|| cfgfile_intval (option, value, _T("megachipmem_size"), &p->z3chipmem_size, 0x100000)
		|| cfgfile_intval (option, value, _T("z3mem_start"), &p->z3autoconfig_start, 1)
		|| cfgfile_intval (option, value, _T("bogomem_size"), &p->bogomem_size, 0x40000)
		|| cfgfile_intval (option, value, _T("gfxcard_size"), &p->rtgmem_size, 0x100000)
		|| cfgfile_intval(option, value, _T("rtg_modes"), &p->picasso96_modeflags, 1)
		|| cfgfile_intval (option, value, _T("floppy_speed"), &p->floppy_speed, 1)
		|| cfgfile_intval (option, value, _T("cd_speed"), &p->cd_speed, 1)
		|| cfgfile_intval (option, value, _T("floppy_write_length"), &p->floppy_write_length, 1)
		|| cfgfile_intval (option, value, _T("floppy_random_bits_min"), &p->floppy_random_bits_min, 1)
		|| cfgfile_intval (option, value, _T("floppy_random_bits_max"), &p->floppy_random_bits_max, 1)
		|| cfgfile_intval (option, value, _T("nr_floppies"), &p->nr_floppies, 1)
		|| cfgfile_intval (option, value, _T("floppy0type"), &p->floppyslots[0].dfxtype, 1)
		|| cfgfile_intval (option, value, _T("floppy1type"), &p->floppyslots[1].dfxtype, 1)
		|| cfgfile_intval (option, value, _T("floppy2type"), &p->floppyslots[2].dfxtype, 1)
		|| cfgfile_intval (option, value, _T("floppy3type"), &p->floppyslots[3].dfxtype, 1)
		|| cfgfile_intval (option, value, _T("maprom"), &p->maprom, 1)
		|| cfgfile_intval (option, value, _T("parallel_autoflush"), &p->parallel_autoflush_time, 1)
		|| cfgfile_intval (option, value, _T("uae_hide"), &p->uae_hide, 1)
		|| cfgfile_intval (option, value, _T("cpu_frequency"), &p->cpu_frequency, 1)
		|| cfgfile_intval(option, value, _T("kickstart_ext_rom_file2addr"), &p->romextfile2addr, 1)
		|| cfgfile_intval(option, value, _T("genlock_mix"), &p->genlock_mix, 1)
		|| cfgfile_intval (option, value, _T("catweasel"), &p->catweasel, 1))
		return 1;

	if (cfgfile_strval (option, value, _T("comp_trustbyte"), &p->comptrustbyte, compmode, 0)
		|| cfgfile_strval (option, value, _T("rtc"), &p->cs_rtc, rtctype, 0)
		|| cfgfile_strval (option, value, _T("ciaatod"), &p->cs_ciaatod, ciaatodmode, 0)
		|| cfgfile_strval (option, value, _T("ide"), &p->cs_ide, idemode, 0)
		|| cfgfile_strval (option, value, _T("scsi"), &p->scsi, scsimode, 0)
		|| cfgfile_strval (option, value, _T("comp_trustword"), &p->comptrustword, compmode, 0)
		|| cfgfile_strval (option, value, _T("comp_trustlong"), &p->comptrustlong, compmode, 0)
		|| cfgfile_strval (option, value, _T("comp_trustnaddr"), &p->comptrustnaddr, compmode, 0)
		|| cfgfile_strval (option, value, _T("collision_level"), &p->collision_level, collmode, 0)
		|| cfgfile_strval (option, value, _T("parallel_matrix_emulation"), &p->parallel_matrix_emulation, epsonprinter, 0)
		|| cfgfile_strval(option, value, _T("monitoremu"), &p->monitoremu, specialmonitors, 0)
		|| cfgfile_strval(option, value, _T("genlockmode"), &p->genlock_image, genlockmodes, 0)
		|| cfgfile_strval (option, value, _T("waiting_blits"), &p->waiting_blits, waitblits, 0)
		|| cfgfile_strval (option, value, _T("floppy_auto_extended_adf"), &p->floppy_auto_ext2, autoext2, 0)
		|| cfgfile_strval (option, value,  _T("z3mapping"), &p->z3_mapping_mode, z3mapping, 0)
		|| cfgfile_strval (option, value,  _T("scsidev_mode"), &p->uaescsidevmode, uaescsidevmodes, 0)
		|| cfgfile_strval (option, value,  _T("boot_rom_uae"), &p->boot_rom, uaebootrom, 0)
		|| cfgfile_strval (option, value,  _T("serial_translate"), &p->serial_crlf, serialcrlf, 0)
		|| cfgfile_strboolval (option, value, _T("comp_flushmode"), &p->comp_hardflush, flushmode, 0))
		return 1;

	if (cfgfile_path (option, value, _T("kickstart_rom_file"), p->romfile, sizeof p->romfile / sizeof (TCHAR), &p->path_rom)
		|| cfgfile_path (option, value, _T("kickstart_ext_rom_file"), p->romextfile, sizeof p->romextfile / sizeof (TCHAR), &p->path_rom)
		|| cfgfile_path (option, value, _T("kickstart_ext_rom_file2"), p->romextfile2, sizeof p->romextfile2 / sizeof (TCHAR), &p->path_rom)
		|| cfgfile_rom(option, value, _T("kickstart_rom_file_id"), p->romfile, sizeof p->romfile / sizeof(TCHAR))
		|| cfgfile_rom (option, value, _T("kickstart_ext_rom_file_id"), p->romextfile, sizeof p->romextfile / sizeof (TCHAR))
		|| cfgfile_path (option, value, _T("flash_file"), p->flashfile, sizeof p->flashfile / sizeof (TCHAR), &p->path_rom)
		|| cfgfile_path (option, value, _T("cart_file"), p->cartfile, sizeof p->cartfile / sizeof (TCHAR), &p->path_rom)
		|| cfgfile_path (option, value, _T("rtc_file"), p->rtcfile, sizeof p->rtcfile / sizeof (TCHAR), &p->path_rom)
		|| cfgfile_path(option, value, _T("picassoiv_rom_file"), p->picassoivromfile, sizeof p->picassoivromfile / sizeof(TCHAR), &p->path_rom)
		|| cfgfile_string(option, value, _T("genlock_image"), p->genlock_image_file, sizeof p->genlock_image_file / sizeof(TCHAR))
		|| cfgfile_string(option, value, _T ("pci_devices"), p->pci_devices, sizeof p->pci_devices / sizeof(TCHAR))
		|| cfgfile_string (option, value, _T("ghostscript_parameters"), p->ghostscript_parameters, sizeof p->ghostscript_parameters / sizeof (TCHAR)))
		return 1;

	if (cfgfile_string(option, value, _T("gfxcard_type"), tmpbuf, sizeof tmpbuf / sizeof(TCHAR))) {
		p->rtgmem_type = 0;
		i = 0;
		for (;;) {
			const TCHAR *t = gfxboard_get_configname(i);
			if (!t)
				break;
			if (!_tcsicmp(t, tmpbuf)) {
				p->rtgmem_type = i;
				break;
			}
			i++;
		}
		return 1;
	}


	if (cfgfile_string(option, value, _T("cpuboard_type"), tmpbuf, sizeof tmpbuf / sizeof(TCHAR))) {
		p->cpuboard_type = 0;
		p->cpuboard_subtype = 0;
		for (i = 0; cpuboards[i].name && !p->cpuboard_type; i++) {
			const struct cpuboardtype *cbt = &cpuboards[i];
			if (cbt->subtypes) {
				for (int j = 0; cbt->subtypes[j].name; j++) {
					if (!_tcsicmp(cbt->subtypes[j].configname, tmpbuf)) {
						p->cpuboard_type = i;
						p->cpuboard_subtype = j;
					}
				}
			}
		}
		return 1;
	}
	if (cfgfile_string(option, value, _T("cpuboard_settings"), tmpbuf, sizeof tmpbuf / sizeof(TCHAR))) {
		p->cpuboard_settings = 0;
		const struct cpuboardsubtype *cbst = &cpuboards[p->cpuboard_type].subtypes[p->cpuboard_subtype];
		if (cbst->settings) {
			const struct expansionboardsettings *cbs = cbst->settings;
			for(i = 0; cbs[i].name; i++) {
				if (cfgfile_option_find(tmpbuf, cbs[i].configname)) {
					p->cpuboard_settings |= 1 << i;
					break;
				}
			}
		}
		return 1;
	}

	if (cfgfile_strval (option, value, _T("chipset_compatible"), &p->cs_compatible, cscompa, 0)) {
		built_in_chipset_prefs (p);
		return 1;
	}

	if (cfgfile_strval (option, value, _T("cart_internal"), &p->cart_internal, cartsmode, 0)) {
		if (p->cart_internal) {
			struct romdata *rd = getromdatabyid (63);
			if (rd)
				_stprintf (p->cartfile, _T(":%s"), rd->configname);
		}
		return 1;
	}
	if (cfgfile_string (option, value, _T("kickstart_rom"), p->romident, sizeof p->romident / sizeof (TCHAR))) {
		decode_rom_ident (p->romfile, sizeof p->romfile / sizeof (TCHAR), p->romident, ROMTYPE_ALL_KICK);
		return 1;
	}
	if (cfgfile_string (option, value, _T("kickstart_ext_rom"), p->romextident, sizeof p->romextident / sizeof (TCHAR))) {
		decode_rom_ident (p->romextfile, sizeof p->romextfile / sizeof (TCHAR), p->romextident, ROMTYPE_ALL_EXT);
		return 1;
	}

	if (cfgfile_string (option, value, _T("cart"), p->cartident, sizeof p->cartident / sizeof (TCHAR))) {
		decode_rom_ident (p->cartfile, sizeof p->cartfile / sizeof (TCHAR), p->cartident, ROMTYPE_ALL_CART);
		return 1;
	}

	if (cfgfile_read_board_rom(p, option, value, &p->path_rom))
		return 1;

	for (i = 0; i < 4; i++) {
		_stprintf (tmpbuf, _T("floppy%d"), i);
		if (cfgfile_path (option, value, tmpbuf, p->floppyslots[i].df, sizeof p->floppyslots[i].df / sizeof (TCHAR), &p->path_floppy))
			return 1;
	}

	if (cfgfile_intval (option, value, _T("chipmem_size"), &dummyint, 1)) {
		if (dummyint < 0)
			p->chipmem_size = 0x20000; /* 128k, prototype support */
		else if (dummyint == 0)
			p->chipmem_size = 0x40000; /* 256k */
		else
			p->chipmem_size = dummyint * 0x80000;
		return 1;
	}

	if (cfgfile_string (option, value, _T("addmem1"), tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) {
		parse_addmem (p, tmpbuf, 0);
		return 1;
	}
	if (cfgfile_string (option, value, _T("addmem2"), tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) {
		parse_addmem (p, tmpbuf, 1);
		return 1;
	}

	if (cfgfile_strval (option, value, _T("chipset"), &tmpval, csmode, 0)) {
		set_chipset_mask (p, tmpval);
		return 1;
	}

	if (cfgfile_string (option, value, _T("mmu_model"), tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) {
		p->mmu_model = _tstol (tmpbuf);
		return 1;
	}

	if (cfgfile_string (option, value, _T("fpu_model"), tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) {
		p->fpu_model = _tstol (tmpbuf);
		return 1;
	}

	if (cfgfile_string (option, value, _T("cpu_model"), tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) {
		p->cpu_model = _tstol (tmpbuf);
		p->fpu_model = 0;
		return 1;
	}

	if (cfgfile_strval(option, value, _T("ppc_implementation"), &p->ppc_implementation, ppc_implementations, 0))
		return 1;
	if (cfgfile_string(option, value, _T("ppc_model"), tmpbuf, sizeof tmpbuf / sizeof(TCHAR))) {
		p->ppc_mode = 0;
		p->ppc_model[0] = 0;
		if (!_tcsicmp(tmpbuf, _T("automatic"))) {
			p->ppc_mode = 1;
		} else if (!_tcsicmp(tmpbuf, _T("manual"))) {
			p->ppc_mode = 2;
		} else {
			if (tmpbuf[0] && _tcslen(tmpbuf) < sizeof(p->ppc_model) / sizeof(TCHAR)) {
				_tcscpy(p->ppc_model, tmpbuf);
				p->ppc_mode = 2;
			}
		}
		return 1;
	}
	if (cfgfile_strval(option, value, _T("ppc_cpu_idle"), &p->ppc_cpu_idle, ppc_cpu_idle, 0))
		return 1;

	/* old-style CPU configuration */
	if (cfgfile_string (option, value, _T("cpu_type"), tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) {
		p->fpu_model = 0;
		p->address_space_24 = 0;
		p->cpu_model = 680000;
		if (!_tcscmp (tmpbuf, _T("68000"))) {
			p->cpu_model = 68000;
		} else if (!_tcscmp (tmpbuf, _T("68010"))) {
			p->cpu_model = 68010;
		} else if (!_tcscmp (tmpbuf, _T("68ec020"))) {
			p->cpu_model = 68020;
			p->address_space_24 = 1;
		} else if (!_tcscmp (tmpbuf, _T("68020"))) {
			p->cpu_model = 68020;
		} else if (!_tcscmp (tmpbuf, _T("68ec020/68881"))) {
			p->cpu_model = 68020;
			p->fpu_model = 68881;
			p->address_space_24 = 1;
		} else if (!_tcscmp (tmpbuf, _T("68020/68881"))) {
			p->cpu_model = 68020;
			p->fpu_model = 68881;
		} else if (!_tcscmp (tmpbuf, _T("68040"))) {
			p->cpu_model = 68040;
			p->fpu_model = 68040;
		} else if (!_tcscmp (tmpbuf, _T("68060"))) {
			p->cpu_model = 68060;
			p->fpu_model = 68060;
		}
		return 1;
	}

	/* Broken earlier versions used to write this out as a string.  */
	if (cfgfile_strval (option, value, _T("finegraincpu_speed"), &p->m68k_speed, speedmode, 1)) {
		p->m68k_speed--;
		return 1;
	}

	if (cfgfile_strval (option, value, _T("cpu_speed"), &p->m68k_speed, speedmode, 1)) {
		p->m68k_speed--;
		return 1;
	}
	if (cfgfile_intval (option, value, _T("cpu_speed"), &p->m68k_speed, 1)) {
		p->m68k_speed *= CYCLE_UNIT;
		return 1;
	}
	if (cfgfile_doubleval (option, value, _T("cpu_throttle"), &p->m68k_speed_throttle)) {
		return 1;
	}
	if (cfgfile_intval (option, value, _T("finegrain_cpu_speed"), &p->m68k_speed, 1)) {
		if (OFFICIAL_CYCLE_UNIT > CYCLE_UNIT) {
			int factor = OFFICIAL_CYCLE_UNIT / CYCLE_UNIT;
			p->m68k_speed = (p->m68k_speed + factor - 1) / factor;
		}
		if (strcasecmp (value, _T("max")) == 0)
			p->m68k_speed = -1;
		return 1;
	}

	if (cfgfile_intval (option, value, _T("dongle"), &p->dongle, 1)) {
		if (p->dongle == 0)
			cfgfile_strval (option, value, _T("dongle"), &p->dongle, dongles, 0);
		return 1;
	}

	if (strcasecmp (option, _T("quickstart")) == 0) {
		int model = 0;
		TCHAR *tmpp = _tcschr (value, ',');
		if (tmpp) {
			*tmpp++ = 0;
			TCHAR *tmpp2 = _tcschr (value, ',');
			if (tmpp2)
				*tmpp2 = 0;
			cfgfile_strval (option, value, option, &model, qsmodes,  0);
			if (model >= 0) {
				int config = _tstol (tmpp);
				built_in_prefs (p, model, config, 0, 0);
			}
		}
		return 1;
	}

	if (cfgfile_parse_filesys (p, option, value))
		return 1;

	return 0;
}


static bool createconfigstore (struct uae_prefs*);
static int getconfigstoreline (const TCHAR *option, TCHAR *value);

static void calcformula (struct uae_prefs *prefs, TCHAR *in)
{
	TCHAR out[MAX_DPATH], configvalue[CONFIG_BLEN];
	TCHAR *p = out;
	double val;
	int cnt1, cnt2;
	static bool updatestore;

	if (_tcslen (in) < 2 || in[0] != '[' || in[_tcslen (in) - 1] != ']')
		return;
	if (!configstore || updatestore)
		createconfigstore (prefs);
	updatestore = false;
	if (!configstore)
		return;
	cnt1 = cnt2 = 0;
	for (int i = 1; i < _tcslen (in) - 1; i++) {
		TCHAR c = _totupper (in[i]);
		if (c >= 'A' && c <='Z') {
			TCHAR *start = &in[i];
			while (_istalnum (c) || c == '_' || c == '.') {
				i++;
				c = in[i];
			}
			TCHAR store = in[i];
			in[i] = 0;
			//write_log (_T("'%s'\n"), start);
			if (!getconfigstoreline (start, configvalue))
				return;
			_tcscpy (p, configvalue);
			p += _tcslen (p);
			in[i] = store;
			i--;
			cnt1++;
		} else {
			cnt2++;
			*p ++= c;
		}
	}
	*p = 0;
	if (cnt1 == 0 && cnt2 == 0)
		return;
	/* single config entry only? */
	if (cnt1 == 1 && cnt2 == 0) {
		_tcscpy (in, out);
		updatestore = true;
		return;
	}
	if (calc (out, &val)) {
		if (val - (int)val != 0.0f)
			_stprintf (in, _T("%f"), val);
		else
			_stprintf (in, _T("%d"), (int)val);
		updatestore = true;
		return;
	}
}

int cfgfile_parse_option (struct uae_prefs *p, const TCHAR *option, TCHAR *value, int type)
{
	calcformula (p, value);

	if (!_tcscmp (option, _T("debug"))) {
		write_log (_T("CONFIG DEBUG: '%s'\n"), value);
		return 1;
	}
	if (!_tcscmp (option, _T("config_hardware")))
		return 1;
	if (!_tcscmp (option, _T("config_host")))
		return 1;
	if (cfgfile_path (option, value, _T("config_hardware_path"), p->config_hardware_path, sizeof p->config_hardware_path / sizeof (TCHAR)))
		return 1;
	if (cfgfile_path (option, value, _T("config_host_path"), p->config_host_path, sizeof p->config_host_path / sizeof (TCHAR)))
		return 1;
	if (type == 0 || (type & CONFIG_TYPE_HARDWARE)) {
		if (cfgfile_parse_hardware (p, option, value))
			return 1;
	}
	if (type == 0 || (type & CONFIG_TYPE_HOST)) {
		// cfgfile_parse_host may modify the option (convert to lowercase).
		TCHAR* writable_option = my_strdup(option);
		if (cfgfile_parse_host (p, writable_option, value)) {
			xfree(writable_option);
			return 1;
		}
		xfree(writable_option);
	}
	if (type > 0 && (type & (CONFIG_TYPE_HARDWARE | CONFIG_TYPE_HOST)) != (CONFIG_TYPE_HARDWARE | CONFIG_TYPE_HOST))
		return 1;
	return 0;
}

static int isutf8ext (TCHAR *s)
{
	if (_tcslen (s) > _tcslen (UTF8NAME) && !_tcscmp (s + _tcslen (s) - _tcslen (UTF8NAME), UTF8NAME)) {
		s[_tcslen (s) - _tcslen (UTF8NAME)] = 0;
		return 1;
	}
	return 0;
}

static int cfgfile_separate_linea (const TCHAR *filename, char *line, TCHAR *line1b, TCHAR *line2b)
{
	char *line1, *line2;
	int i;

	line1 = line;
	line1 += strspn (line1, "\t \r\n");
	if (*line1 == ';')
		return 0;
	line2 = strchr (line, '=');
	if (! line2) {
		TCHAR *s = au (line1);
		write_log (_T("CFGFILE: '%s', linea was incomplete with only %s\n"), filename, s);
		xfree (s);
		return 0;
	}
	*line2++ = '\0';

	/* Get rid of whitespace.  */
	i = strlen (line2);
	while (i > 0 && (line2[i - 1] == '\t' || line2[i - 1] == ' '
		|| line2[i - 1] == '\r' || line2[i - 1] == '\n'))
		line2[--i] = '\0';
	line2 += strspn (line2, "\t \r\n");

	i = strlen (line);
	while (i > 0 && (line[i - 1] == '\t' || line[i - 1] == ' '
		|| line[i - 1] == '\r' || line[i - 1] == '\n'))
		line[--i] = '\0';
	line += strspn (line, "\t \r\n");
	au_copy (line1b, MAX_DPATH, line);
	if (isutf8ext (line1b)) {
		if (line2[0]) {
			TCHAR *s = utf8u (line2);
			_tcscpy (line2b, s);
			xfree (s);
		}
	} else {
		au_copy (line2b, MAX_DPATH, line2);
	}
	return 1;
}

static int cfgfile_separate_line (TCHAR *line, TCHAR *line1b, TCHAR *line2b)
{
	TCHAR *line1, *line2;
	int i;

	line1 = line;
	line1 += _tcsspn (line1, _T("\t \r\n"));
	if (*line1 == ';')
		return 0;
	line2 = _tcschr (line, '=');
	if (! line2) {
		write_log (_T("CFGFILE: line was incomplete with only %s\n"), line1);
		return 0;
	}
	*line2++ = '\0';

	/* Get rid of whitespace.  */
	i = _tcslen (line2);
	while (i > 0 && (line2[i - 1] == '\t' || line2[i - 1] == ' '
		|| line2[i - 1] == '\r' || line2[i - 1] == '\n'))
		line2[--i] = '\0';
	line2 += _tcsspn (line2, _T("\t \r\n"));
	_tcscpy (line2b, line2);
	i = _tcslen (line);
	while (i > 0 && (line[i - 1] == '\t' || line[i - 1] == ' '
		|| line[i - 1] == '\r' || line[i - 1] == '\n'))
		line[--i] = '\0';
	line += _tcsspn (line, _T("\t \r\n"));
	_tcscpy (line1b, line);

	if (line2b[0] == '"' || line2b[0] == '\"') {
		TCHAR c = line2b[0];
		int i = 0;
		memmove (line2b, line2b + 1, (_tcslen (line2b) + 1) * sizeof (TCHAR));
		while (line2b[i] != 0 && line2b[i] != c)
			i++;
		line2b[i] = 0;
	}

	if (isutf8ext (line1b))
		return 0;
	return 1;
}

static int isobsolete (TCHAR *s)
{
	int i = 0;
	while (obsolete[i]) {
		if (!strcasecmp (s, obsolete[i])) {
			write_log (_T("obsolete config entry '%s'\n"), s);
			return 1;
		}
		i++;
	}
	if (_tcslen (s) > 2 && !_tcsncmp (s, _T("w."), 2))
		return 1;
	if (_tcslen (s) >= 10 && !_tcsncmp (s, _T("gfx_opengl"), 10)) {
		write_log (_T("obsolete config entry '%s\n"), s);
		return 1;
	}
	if (_tcslen (s) >= 6 && !_tcsncmp (s, _T("gfx_3d"), 6)) {
		write_log (_T("obsolete config entry '%s\n"), s);
		return 1;
	}
	return 0;
}

static void cfgfile_parse_separated_line (struct uae_prefs *p, TCHAR *line1b, TCHAR *line2b, int type)
{
	TCHAR line3b[CONFIG_BLEN], line4b[CONFIG_BLEN];
	struct strlist *sl;
	int ret;

	_tcscpy (line3b, line1b);
	_tcscpy (line4b, line2b);
	ret = cfgfile_parse_option (p, line1b, line2b, type);
	if (!isobsolete (line3b)) {
		for (sl = p->all_lines; sl; sl = sl->next) {
			if (sl->option && !strcasecmp (line1b, sl->option)) break;
		}
		if (!sl) {
			struct strlist *u = xcalloc (struct strlist, 1);
			u->option = my_strdup (line3b);
			u->value = my_strdup (line4b);
			u->next = p->all_lines;
			p->all_lines = u;
			if (!ret) {
				u->unknown = 1;
				write_log (_T("unknown config entry: '%s=%s'\n"), u->option, u->value);
			}
		}
	}
}

void cfgfile_parse_lines (struct uae_prefs *p, const TCHAR *lines, int type)
{
	TCHAR *buf = my_strdup (lines);
	TCHAR *t = buf;
	for (;;) {
		if (_tcslen (t) == 0)
			break;
		TCHAR *t2 = _tcschr (t, '\n');
		if (t2)
			*t2 = 0;
		cfgfile_parse_line (p, t, type);
		if (!t2)
			break;
		t = t2 + 1;
	}
	xfree (buf);
}

void cfgfile_parse_line (struct uae_prefs *p, TCHAR *line, int type)
{
	TCHAR line1b[CONFIG_BLEN], line2b[CONFIG_BLEN];

	if (!cfgfile_separate_line (line, line1b, line2b))
		return;
	cfgfile_parse_separated_line (p, line1b, line2b, type);
}

static void subst (TCHAR *p, TCHAR *f, int n)
{
	if (_tcslen(p) == 0 || _tcslen(f) == 0)
		return;
	TCHAR *str = cfgfile_subst_path (UNEXPANDED, p, f);
	_tcsncpy (f, str, n - 1);
	f[n - 1] = '\0';
	free (str);
}

static int getconfigstoreline (const TCHAR *option, TCHAR *value)
{
	TCHAR tmp[CONFIG_BLEN * 2], tmp2[CONFIG_BLEN * 2];
	int idx = 0;

	if (!configstore)
		return 0;
	zfile_fseek (configstore, 0, SEEK_SET);
	for (;;) {
		if (!zfile_fgets (tmp, sizeof tmp / sizeof (TCHAR), configstore))
			return 0;
		if (!cfgfile_separate_line (tmp, tmp2, value))
			continue;
		if (!_tcsicmp (option, tmp2))
			return 1;
	}
}

static bool createconfigstore (struct uae_prefs *p)
{
	uae_u8 zeros[4] = { 0 };
	zfile_fclose (configstore);
	configstore = zfile_fopen_empty (NULL, _T("configstore"), 50000);
	if (!configstore)
		return false;
	zfile_fseek (configstore, 0, SEEK_SET);
	uaeconfig++;
	cfgfile_save_options (configstore, p, 0);
	uaeconfig--;
	zfile_fwrite (zeros, 1, sizeof zeros, configstore);
	zfile_fseek (configstore, 0, SEEK_SET);
	return true;
}

static char *cfg_fgets (char *line, int max, struct zfile *fh)
{
#ifdef SINGLEFILE
	extern TCHAR singlefile_config[];
	static TCHAR *sfile_ptr;
	TCHAR *p;
#endif

	if (fh)
		return zfile_fgetsa (line, max, fh);
#ifdef SINGLEFILE
	if (sfile_ptr == 0) {
		sfile_ptr = singlefile_config;
		if (*sfile_ptr) {
			write_log (_T("singlefile config found\n"));
			while (*sfile_ptr++);
		}
	}
	if (*sfile_ptr == 0) {
		sfile_ptr = singlefile_config;
		return 0;
	}
	p = sfile_ptr;
	while (*p != 13 && *p != 10 && *p != 0) p++;
	memset (line, 0, max);
	memcpy (line, sfile_ptr, (p - sfile_ptr) * sizeof (TCHAR));
	sfile_ptr = p + 1;
	if (*sfile_ptr == 13)
		sfile_ptr++;
	if (*sfile_ptr == 10)
		sfile_ptr++;
	return line;
#endif
	return 0;
}

static int cfgfile_load_2 (struct uae_prefs *p, const TCHAR *filename, bool real, int *type)
{
	int i;
	struct zfile *fh;
	char linea[CONFIG_BLEN];
	TCHAR line[CONFIG_BLEN], line1b[CONFIG_BLEN], line2b[CONFIG_BLEN];
	struct strlist *sl;
	bool type1 = false, type2 = false;
	int askedtype = 0;

	if (type) {
		askedtype = *type;
		*type = 0;
	}
	if (real) {
		p->config_version = 0;
		config_newfilesystem = 0;
		store_inputdevice_config (p);
		//reset_inputdevice_config (p);
	}

	fh = zfile_fopen (filename, _T("r"), ZFD_NORMAL);
#ifndef	SINGLEFILE
	if (! fh)
		return 0;
#endif

	while (cfg_fgets (linea, sizeof (linea), fh) != 0) {
		trimwsa (linea);
		if (strlen (linea) > 0) {
			if (linea[0] == '#' || linea[0] == ';') {
				struct strlist *u = xcalloc (struct strlist, 1);
				u->option = NULL;
				TCHAR *com = au (linea);
				u->value = my_strdup (com);
				xfree (com);
				u->unknown = 1;
				u->next = p->all_lines;
				p->all_lines = u;
				continue;
			}
			if (!cfgfile_separate_linea (filename, linea, line1b, line2b))
				continue;
			type1 = type2 = 0;
			if (cfgfile_yesno (line1b, line2b, _T("config_hardware"), &type1) ||
				cfgfile_yesno (line1b, line2b, _T("config_host"), &type2)) {
					if (type1 && type)
						*type |= CONFIG_TYPE_HARDWARE;
					if (type2 && type)
						*type |= CONFIG_TYPE_HOST;
					continue;
			}
			if (real) {
				cfgfile_parse_separated_line (p, line1b, line2b, askedtype);
			} else {
				cfgfile_string (line1b, line2b, _T("config_description"), p->description, sizeof p->description / sizeof (TCHAR));
				cfgfile_path (line1b, line2b, _T("config_hardware_path"), p->config_hardware_path, sizeof p->config_hardware_path / sizeof (TCHAR));
				cfgfile_path (line1b, line2b, _T("config_host_path"), p->config_host_path, sizeof p->config_host_path / sizeof (TCHAR));
				cfgfile_string (line1b, line2b, _T("config_window_title"), p->config_window_title, sizeof p->config_window_title / sizeof (TCHAR));
			}
		}
	}

	if (type && *type == 0)
		*type = CONFIG_TYPE_HARDWARE | CONFIG_TYPE_HOST;
	zfile_fclose (fh);

	if (!real)
		return 1;

	for (sl = temp_lines; sl; sl = sl->next) {
		_stprintf (line, _T("%s=%s"), sl->option, sl->value);
		cfgfile_parse_line (p, line, 0);
	}

	for (i = 0; i < 4; i++)
		subst (p->path_floppy.path[0], p->floppyslots[i].df, sizeof p->floppyslots[i].df / sizeof (TCHAR));
	subst (p->path_rom.path[0], p->romfile, sizeof p->romfile / sizeof (TCHAR));
	subst (p->path_rom.path[0], p->romextfile, sizeof p->romextfile / sizeof (TCHAR));
	subst (p->path_rom.path[0], p->romextfile2, sizeof p->romextfile2 / sizeof (TCHAR));

	for (i = 0; i < MAX_EXPANSION_BOARDS; i++) {
		for (int j = 0; j < MAX_BOARD_ROMS; j++) {
			subst(p->path_rom.path[0], p->expansionboard[i].roms[j].romfile, MAX_DPATH / sizeof(TCHAR));
		}
	}

	return 1;
}

int cfgfile_load (struct uae_prefs *p, const TCHAR *filename, int *type, int ignorelink, int userconfig)
{
	int v;
	TCHAR tmp[MAX_DPATH];
	int type2;
	static int recursive;

	if (recursive > 1)
		return 0;
	recursive++;
	write_log (_T("load config '%s':%d\n"), filename, type ? *type : -1);
	v = cfgfile_load_2 (p, filename, 1, type);
	if (!v) {
		write_log (_T("load failed\n"));
		goto end;
	}
	if (userconfig)
		target_addtorecent (filename, 0);
	if (!ignorelink) {
		if (p->config_hardware_path[0]) {
			fetch_configurationpath (tmp, sizeof (tmp) / sizeof (TCHAR));
			_tcsncat (tmp, p->config_hardware_path, sizeof (tmp) / sizeof (TCHAR));
			type2 = CONFIG_TYPE_HARDWARE;
			cfgfile_load (p, tmp, &type2, 1, 0);
		}
		if (p->config_host_path[0]) {
			fetch_configurationpath (tmp, sizeof (tmp) / sizeof (TCHAR));
			_tcsncat (tmp, p->config_host_path, sizeof (tmp) / sizeof (TCHAR));
			type2 = CONFIG_TYPE_HOST;
			cfgfile_load (p, tmp, &type2, 1, 0);
		}
	}
end:
	recursive--;
	fixup_prefs (p);
	return v;
}

void cfgfile_backup (const TCHAR *path)
{
	TCHAR dpath[MAX_DPATH];

	fetch_configurationpath (dpath, sizeof (dpath) / sizeof (TCHAR));
	_tcscat (dpath, _T("configuration.backup"));
	bool hidden = my_isfilehidden (dpath);
	my_unlink (dpath);
	my_rename (path, dpath);
	if (hidden)
		my_setfilehidden (dpath, hidden);
}

int cfgfile_save (struct uae_prefs *p, const TCHAR *filename, int type)
{
	struct zfile *fh;

	cfgfile_backup (filename);
	fh = zfile_fopen (filename, unicode_config ? _T("w, ccs=UTF-8") : _T("w"), ZFD_NORMAL);
	if (! fh)
		return 0;

	if (!type)
		type = CONFIG_TYPE_HARDWARE | CONFIG_TYPE_HOST;
	cfgfile_save_options (fh, p, type);
	zfile_fclose (fh);
	return 1;
}

int cfgfile_get_description (const TCHAR *filename, TCHAR *description, TCHAR *hostlink, TCHAR *hardwarelink, int *type)
{
	int result = 0;
	struct uae_prefs *p = xmalloc (struct uae_prefs, 1);

	p->description[0] = 0;
	p->config_host_path[0] = 0;
	p->config_hardware_path[0] = 0;
	if (cfgfile_load_2 (p, filename, 0, type)) {
		result = 1;
		if (description)
			_tcscpy (description, p->description);
		if (hostlink)
			_tcscpy (hostlink, p->config_host_path);
		if (hardwarelink)
			_tcscpy (hardwarelink, p->config_hardware_path);
	}
	xfree (p);
	return result;
}

int cfgfile_configuration_change (int v)
{
	static int mode;
	if (v >= 0)
		mode = v;
	return mode;
}

void cfgfile_show_usage (void)
{
	int i;
	write_log (_T("UAE Configuration Help:\n") \
		_T("=======================\n"));
	for (i = 0; i < sizeof opttable / sizeof *opttable; i++)
		write_log (_T("%s: %s\n"), opttable[i].config_label, opttable[i].config_help);
}

/* This implements the old commandline option parsing. I've re-added this
because the new way of doing things is painful for me (it requires me
to type a couple hundred characters when invoking UAE).  The following
is far less annoying to use.  */
static void parse_gfx_specs (struct uae_prefs *p, const TCHAR *spec)
{
	TCHAR *x0 = my_strdup (spec);
	TCHAR *x1, *x2;

	x1 = _tcschr (x0, ':');
	if (x1 == 0)
		goto argh;
	x2 = _tcschr (x1+1, ':');
	if (x2 == 0)
		goto argh;
	*x1++ = 0; *x2++ = 0;

	p->gfx_size_win.width = p->gfx_size_fs.width = _tstoi (x0);
	p->gfx_size_win.height = p->gfx_size_fs.height = _tstoi (x1);
	p->gfx_resolution = _tcschr (x2, 'l') != 0 ? 1 : 0;
	p->gfx_xcenter = _tcschr (x2, 'x') != 0 ? 1 : _tcschr (x2, 'X') != 0 ? 2 : 0;
	p->gfx_ycenter = _tcschr (x2, 'y') != 0 ? 1 : _tcschr (x2, 'Y') != 0 ? 2 : 0;
	p->gfx_vresolution = _tcschr (x2, 'd') != 0 ? VRES_DOUBLE : VRES_NONDOUBLE;
	p->gfx_pscanlines = _tcschr (x2, 'D') != 0;
	if (p->gfx_pscanlines)
		p->gfx_vresolution = VRES_DOUBLE;
	p->gfx_apmode[0].gfx_fullscreen = _tcschr (x2, 'a') != 0;
	p->gfx_apmode[1].gfx_fullscreen = _tcschr (x2, 'p') != 0;

	free (x0);
	return;

argh:
	write_log (_T("Bad display mode specification.\n"));
	write_log (_T("The format to use is: \"width:height:modifiers\"\n"));
	write_log (_T("Type \"uae -h\" for detailed help.\n"));
	free (x0);
}

static void parse_sound_spec (struct uae_prefs *p, const TCHAR *spec)
{
	TCHAR *x0 = my_strdup (spec);
	TCHAR *x1, *x2 = NULL, *x3 = NULL, *x4 = NULL, *x5 = NULL;

	x1 = _tcschr (x0, ':');
	if (x1 != NULL) {
		*x1++ = '\0';
		x2 = _tcschr (x1 + 1, ':');
		if (x2 != NULL) {
			*x2++ = '\0';
			x3 = _tcschr (x2 + 1, ':');
			if (x3 != NULL) {
				*x3++ = '\0';
				x4 = _tcschr (x3 + 1, ':');
				if (x4 != NULL) {
					*x4++ = '\0';
					x5 = _tcschr (x4 + 1, ':');
				}
			}
		}
	}
	p->produce_sound = _tstoi (x0);
	if (x1) {
		p->sound_stereo_separation = 0;
		if (*x1 == 'S') {
			p->sound_stereo = SND_STEREO;
			p->sound_stereo_separation = 7;
		} else if (*x1 == 's')
			p->sound_stereo = SND_STEREO;
		else
			p->sound_stereo = SND_MONO;
	}
	if (x3)
		p->sound_freq = _tstoi (x3);
	if (x4)
		p->sound_maxbsiz = _tstoi (x4);
	free (x0);
}


static void parse_joy_spec (struct uae_prefs *p, const TCHAR *spec)
{
	int v0 = 2, v1 = 0;
	if (_tcslen(spec) != 2)
		goto bad;

	switch (spec[0]) {
	case '0': v0 = JSEM_JOYS; break;
	case '1': v0 = JSEM_JOYS + 1; break;
	case 'M': case 'm': v0 = JSEM_MICE; break;
	case 'A': case 'a': v0 = JSEM_KBDLAYOUT; break;
	case 'B': case 'b': v0 = JSEM_KBDLAYOUT + 1; break;
	case 'C': case 'c': v0 = JSEM_KBDLAYOUT + 2; break;
	default: goto bad;
	}

	switch (spec[1]) {
	case '0': v1 = JSEM_JOYS; break;
	case '1': v1 = JSEM_JOYS + 1; break;
	case 'M': case 'm': v1 = JSEM_MICE; break;
	case 'A': case 'a': v1 = JSEM_KBDLAYOUT; break;
	case 'B': case 'b': v1 = JSEM_KBDLAYOUT + 1; break;
	case 'C': case 'c': v1 = JSEM_KBDLAYOUT + 2; break;
	default: goto bad;
	}
	if (v0 == v1)
		goto bad;
	/* Let's scare Pascal programmers */
	if (0)
bad:
	write_log (_T("Bad joystick mode specification. Use -J xy, where x and y\n")
		_T("can be 0 for joystick 0, 1 for joystick 1, M for mouse, and\n")
		_T("a, b or c for different keyboard settings.\n"));

	p->jports[0].id = v0;
	p->jports[1].id = v1;
}

static void parse_filesys_spec (struct uae_prefs *p, bool readonly, const TCHAR *spec)
{
	struct uaedev_config_info uci;
	TCHAR buf[256];
	TCHAR *s2;

	uci_set_defaults (&uci, false);
	_tcsncpy (buf, spec, 255); buf[255] = 0;
	s2 = _tcschr (buf, ':');
	if (s2) {
		*s2++ = '\0';
#ifdef __DOS__
		{
			TCHAR *tmp;

			while ((tmp = _tcschr (s2, '\\')))
				*tmp = '/';
		}
#endif
#ifdef FILESYS
		_tcscpy (uci.volname, buf);
		_tcscpy (uci.rootdir, s2);
		uci.readonly = readonly;
		uci.type = UAEDEV_DIR;
		add_filesys_config (p, -1, &uci);
#endif
	} else {
		write_log (_T("Usage: [-m | -M] VOLNAME:mount_point\n"));
	}
}

static void parse_hardfile_spec (struct uae_prefs *p, const TCHAR *spec)
{
	struct uaedev_config_info uci;
	TCHAR *x0 = my_strdup (spec);
	TCHAR *x1, *x2, *x3, *x4;

	uci_set_defaults (&uci, false);
	x1 = _tcschr (x0, ':');
	if (x1 == NULL)
		goto argh;
	*x1++ = '\0';
	x2 = _tcschr (x1 + 1, ':');
	if (x2 == NULL)
		goto argh;
	*x2++ = '\0';
	x3 = _tcschr (x2 + 1, ':');
	if (x3 == NULL)
		goto argh;
	*x3++ = '\0';
	x4 = _tcschr (x3 + 1, ':');
	if (x4 == NULL)
		goto argh;
	*x4++ = '\0';
#ifdef FILESYS
	_tcscpy (uci.rootdir, x4);
	//add_filesys_config (p, -1, NULL, NULL, x4, 0, 0, _tstoi (x0), _tstoi (x1), _tstoi (x2), _tstoi (x3), 0, 0, 0, 0, 0, 0, 0);
#endif
	free (x0);
	return;

argh:
	free (x0);
	write_log (_T("Bad hardfile parameter specified - type \"uae -h\" for help.\n"));
	return;
}

static void parse_cpu_specs (struct uae_prefs *p, const TCHAR *spec)
{
	if (*spec < '0' || *spec > '4') {
		write_log (_T("CPU parameter string must begin with '0', '1', '2', '3' or '4'.\n"));
		return;
	}

	p->cpu_model = (*spec++) * 10 + 68000;
	p->address_space_24 = p->cpu_model < 68020;
	p->cpu_compatible = 0;
	while (*spec != '\0') {
		switch (*spec) {
		case 'a':
			if (p->cpu_model < 68020)
				write_log (_T("In 68000/68010 emulation, the address space is always 24 bit.\n"));
			else if (p->cpu_model >= 68040)
				write_log (_T("In 68040/060 emulation, the address space is always 32 bit.\n"));
			else
				p->address_space_24 = 1;
			break;
		case 'c':
			if (p->cpu_model != 68000)
				write_log (_T("The more compatible CPU emulation is only available for 68000\n")
				_T("emulation, not for 68010 upwards.\n"));
			else
				p->cpu_compatible = 1;
			break;
		default:
			write_log (_T("Bad CPU parameter specified - type \"uae -h\" for help.\n"));
			break;
		}
		spec++;
	}
}

static void cmdpath (TCHAR *dst, const TCHAR *src, int maxsz)
{
	TCHAR *s = target_expand_environment (src);
	_tcsncpy (dst, s, maxsz);
	dst[maxsz] = 0;
	xfree (s);
}

/* Returns the number of args used up (0 or 1).  */
int parse_cmdline_option (struct uae_prefs *p, TCHAR c, const TCHAR *arg)
{
	struct strlist *u = xcalloc (struct strlist, 1);
	const TCHAR arg_required[] = _T("0123rKpImWSAJwNCZUFcblOdHRv");

	if (_tcschr (arg_required, c) && ! arg) {
		write_log (_T("Missing argument for option `-%c'!\n"), c);
		return 0;
	}

	u->option = xmalloc (TCHAR, 2);
	u->option[0] = c;
	u->option[1] = 0;
	u->value = my_strdup (arg);
	u->next = p->all_lines;
	p->all_lines = u;

	switch (c) {
	case 'h': usage (); exit (0);

	case '0': cmdpath (p->floppyslots[0].df, arg, 255); break;
	case '1': cmdpath (p->floppyslots[1].df, arg, 255); break;
	case '2': cmdpath (p->floppyslots[2].df, arg, 255); break;
	case '3': cmdpath (p->floppyslots[3].df, arg, 255); break;
	case 'r': cmdpath (p->romfile, arg, 255); break;
	case 'K': cmdpath (p->romextfile, arg, 255); break;
	case 'p': _tcsncpy (p->prtname, arg, 255); p->prtname[255] = 0; break;
		/*     case 'I': _tcsncpy (p->sername, arg, 255); p->sername[255] = 0; currprefs.use_serial = 1; break; */
	case 'm': case 'M': parse_filesys_spec (p, c == 'M', arg); break;
	case 'W': parse_hardfile_spec (p, arg); break;
	case 'S': parse_sound_spec (p, arg); break;
	case 'R': p->gfx_framerate = _tstoi (arg); break;
	case 'i': p->illegal_mem = 1; break;
	case 'J': parse_joy_spec (p, arg); break;

	case 'w': p->m68k_speed = _tstoi (arg); break;

		/* case 'g': p->use_gfxlib = 1; break; */
	case 'G': p->start_gui = 0; break;
	case 'D': p->start_debugger = 1; break;

	case 'n':
		if (_tcschr (arg, 'i') != 0)
			p->immediate_blits = 1;
		break;

	case 'v':
		set_chipset_mask (p, _tstoi (arg));
		break;

	case 'C':
		parse_cpu_specs (p, arg);
		break;

	case 'Z':
		p->z3fastmem_size = _tstoi (arg) * 0x100000;
		break;

	case 'U':
		p->rtgmem_size = _tstoi (arg) * 0x100000;
		break;

	case 'F':
		p->fastmem_size = _tstoi (arg) * 0x100000;
		break;

	case 'b':
		p->bogomem_size = _tstoi (arg) * 0x40000;
		break;

	case 'c':
		p->chipmem_size = _tstoi (arg) * 0x80000;
		break;

	case 'l':
		if (0 == strcasecmp(arg, _T("de")))
			p->keyboard_lang = KBD_LANG_DE;
		else if (0 == strcasecmp(arg, _T("dk")))
			p->keyboard_lang = KBD_LANG_DK;
		else if (0 == strcasecmp(arg, _T("us")))
			p->keyboard_lang = KBD_LANG_US;
		else if (0 == strcasecmp(arg, _T("se")))
			p->keyboard_lang = KBD_LANG_SE;
		else if (0 == strcasecmp(arg, _T("fr")))
			p->keyboard_lang = KBD_LANG_FR;
		else if (0 == strcasecmp(arg, _T("it")))
			p->keyboard_lang = KBD_LANG_IT;
		else if (0 == strcasecmp(arg, _T("es")))
			p->keyboard_lang = KBD_LANG_ES;
		break;

	case 'O': parse_gfx_specs (p, arg); break;
	case 'd':
		if (_tcschr (arg, 'S') != NULL || _tcschr (arg, 's')) {
			write_log (_T("  Serial on demand.\n"));
			p->serial_demand = 1;
		}
		if (_tcschr (arg, 'P') != NULL || _tcschr (arg, 'p')) {
			write_log (_T("  Parallel on demand.\n"));
			p->parallel_demand = 1;
		}

		break;

	case 'H':
		p->color_mode = _tstoi (arg);
		if (p->color_mode < 0) {
			write_log (_T("Bad color mode selected. Using default.\n"));
			p->color_mode = 0;
		}
		break;
	default:
		write_log (_T("Unknown option `-%c'!\n"), c);
		break;
	}
	return !! _tcschr (arg_required, c);
}

void cfgfile_addcfgparam (TCHAR *line)
{
	struct strlist *u;
	TCHAR line1b[CONFIG_BLEN], line2b[CONFIG_BLEN];

	if (!line) {
		struct strlist **ps = &temp_lines;
		while (*ps) {
			struct strlist *s = *ps;
			*ps = s->next;
			xfree (s->value);
			xfree (s->option);
			xfree (s);
		}
		temp_lines = 0;
		return;
	}
	if (!cfgfile_separate_line (line, line1b, line2b))
		return;
	u = xcalloc (struct strlist, 1);
	u->option = my_strdup (line1b);
	u->value = my_strdup (line2b);
	u->next = temp_lines;
	temp_lines = u;
}

#if 0
static int cfgfile_handle_custom_event (TCHAR *custom, int mode)
{
	TCHAR option[CONFIG_BLEN], value[CONFIG_BLEN];
	TCHAR option2[CONFIG_BLEN], value2[CONFIG_BLEN];
	TCHAR *tmp, *p, *nextp;
	struct zfile *configstore = NULL;
	int cnt = 0, cnt_ok = 0;

	if (!mode) {
		TCHAR zero = 0;
		configstore = zfile_fopen_empty ("configstore", 50000);
		cfgfile_save_options (configstore, &currprefs, 0);
		cfg_write (&zero, configstore);
	}

	nextp = NULL;
	tmp = p = xcalloc (TCHAR, _tcslen (custom) + 2);
	_tcscpy (tmp, custom);
	while (p && *p) {
		if (*p == '\"') {
			TCHAR *p2;
			p++;
			p2 = p;
			while (*p2 != '\"' && *p2 != 0)
				p2++;
			if (*p2 == '\"') {
				*p2++ = 0;
				nextp = p2 + 1;
				if (*nextp == ' ')
					nextp++;
			}
		}
		if (cfgfile_separate_line (p, option, value)) {
			cnt++;
			if (mode) {
				cfgfile_parse_option (&changed_prefs, option, value, 0);
			} else {
				zfile_fseek (configstore, 0, SEEK_SET);
				for (;;) {
					if (!getconfigstoreline (configstore, option2, value2))
						break;
					if (!_tcscmpi (option, option2) && !_tcscmpi (value, value2)) {
						cnt_ok++;
						break;
					}
				}
			}
		}
		p = nextp;
	}
	xfree (tmp);
	zfile_fclose (configstore);
	if (cnt > 0 && cnt == cnt_ok)
		return 1;
	return 0;
}
#endif

int cmdlineparser (const TCHAR *s, TCHAR *outp[], int max)
{
	int j, cnt = 0;
	int slash = 0;
	int quote = 0;
	TCHAR tmp1[MAX_DPATH];
	const TCHAR *prev;
	int doout;

	doout = 0;
	prev = s;
	j = 0;
	outp[0] = 0;
	while (cnt < max) {
		TCHAR c = *s++;
		if (!c)
			break;
		if (c < 32)
			continue;
		if (c == '\\')
			slash = 1;
		if (!slash && c == '"') {
			if (quote) {
				quote = 0;
				doout = 1;
			} else {
				quote = 1;
				j = -1;
			}
		}
		if (!quote && c == ' ')
			doout = 1;
		if (!doout) {
			if (j >= 0) {
				tmp1[j] = c;
				tmp1[j + 1] = 0;
			}
			j++;
		}
		if (doout) {
			if (_tcslen (tmp1) > 0) {
				outp[cnt++] = my_strdup (tmp1);
				outp[cnt] = 0;
			}
			tmp1[0] = 0;
			doout = 0;
			j = 0;
		}
		slash = 0;
	}
	if (j > 0 && cnt < max) {
		outp[cnt++] = my_strdup (tmp1);
		outp[cnt] = 0;
	}
	return cnt;
}

#define UAELIB_MAX_PARSE 100

static bool cfgfile_parse_uaelib_option (struct uae_prefs *p, TCHAR *option, TCHAR *value, int type)
{
	return false;
}

int cfgfile_searchconfig(const TCHAR *in, int index, TCHAR *out, int outsize)
{
	TCHAR tmp[CONFIG_BLEN];
	int j = 0;
	int inlen = _tcslen (in);
	int joker = 0;
	uae_u32 err = 0;
	bool configsearchfound = false;

	if (in[inlen - 1] == '*') {
		joker = 1;
		inlen--;
	}
	*out = 0;

	if (!configstore)
		createconfigstore(&currprefs);
	if (!configstore)
		return 20;

	if (index < 0)
		zfile_fseek(configstore, 0, SEEK_SET);

	for (;;) {
		uae_u8 b = 0;

		if (zfile_fread (&b, 1, 1, configstore) != 1) {
			err = 10;
			if (configsearchfound)
				err = 0;
			goto end;
		}
		if (j >= sizeof (tmp) / sizeof (TCHAR) - 1)
			j = sizeof (tmp) / sizeof (TCHAR) - 1;
		if (b == 0) {
			err = 10;
			if (configsearchfound)
				err = 0;
			goto end;
		}
		if (b == '\n') {
			if (!_tcsncmp (tmp, in, inlen) && ((inlen > 0 && _tcslen (tmp) > inlen && tmp[inlen] == '=') || (joker))) {
				TCHAR *p;
				if (joker)
					p = tmp - 1;
				else
					p = _tcschr (tmp, '=');
				if (p) {
					for (int i = 0; out && i < outsize - 1; i++) {
						TCHAR b = *++p;
						out[i] = b;
						out[i + 1] = 0;
						if (!b)
							break;
					}
				}
				err = 0xffffffff;
				configsearchfound = true;
				goto end;
			}
			j = 0;
		} else {
			tmp[j++] = b;
			tmp[j] = 0;
		}
	}
end:
	return err;
}

uae_u32 cfgfile_modify (uae_u32 index, const TCHAR *parms, uae_u32 size, TCHAR *out, uae_u32 outsize)
{
	TCHAR *p;
	TCHAR *argc[UAELIB_MAX_PARSE];
	int argv, i;
	uae_u32 err;
	TCHAR zero = 0;
	static TCHAR *configsearch;

	*out = 0;
	err = 0;
	argv = 0;
	p = 0;
	if (index != 0xffffffff) {
		if (!configstore) {
			err = 20;
			goto end;
		}
		if (configsearch) {
			err = cfgfile_searchconfig(configsearch, index,  out, outsize);
			goto end;
		}
		err = 0xffffffff;
		for (i = 0; out && i < outsize - 1; i++) {
			uae_u8 b = 0;
			if (zfile_fread (&b, 1, 1, configstore) != 1)
				err = 0;
			if (b == 0)
				err = 0;
			if (b == '\n')
				b = 0;
			out[i] = b;
			out[i + 1] = 0;
			if (!b)
				break;
		}
		goto end;
	}

	if (size > 10000)
		return 10;
	argv = cmdlineparser (parms, argc, UAELIB_MAX_PARSE);

	if (argv <= 1 && index == 0xffffffff) {
		createconfigstore (&currprefs);
		xfree (configsearch);
		configsearch = NULL;
		if (!configstore) {
			err = 20;
			goto end;
		}
		if (argv > 0 && _tcslen (argc[0]) > 0)
			configsearch = my_strdup (argc[0]);
		err = 0xffffffff;
		goto end;
	}

	for (i = 0; i < argv; i++) {
		if (i + 2 <= argv) {
			if (!_tcsicmp (argc[i], _T("dbg"))) {
				debug_parser (argc[i + 1], out, outsize);
			} else if (!inputdevice_uaelib (argc[i], argc[i + 1])) {
				if (!cfgfile_parse_uaelib_option (&changed_prefs, argc[i], argc[i + 1], 0)) {
					if (!cfgfile_parse_option (&changed_prefs, argc[i], argc[i + 1], 0)) {
						err = 5;
						break;
					}
				}
			}
			set_config_changed ();
			set_special(SPCFLAG_MODE_CHANGE);
			i++;
		}
	}
end:
	for (i = 0; i < argv; i++)
		xfree (argc[i]);
	xfree (p);
	return err;
}

uae_u32 cfgfile_uaelib_modify (uae_u32 index, uae_u32 parms, uae_u32 size, uae_u32 out, uae_u32 outsize)
{
	uae_char *p, *parms_p = NULL, *parms_out = NULL;
	int i, ret;
	TCHAR *out_p = NULL, *parms_in = NULL;

	if (out)
		put_byte (out, 0);
	if (size == 0) {
		while (get_byte (parms + size) != 0)
			size++;
	}
	parms_p = xmalloc (uae_char, size + 1);
	if (!parms_p) {
		ret = 10;
		goto end;
	}
	if (out) {
		out_p = xmalloc (TCHAR, outsize + 1);
		if (!out_p) {
			ret = 10;
			goto end;
		}
		out_p[0] = 0;
	}
	p = parms_p;
	for (i = 0; i < size; i++) {
		p[i] = get_byte (parms + i);
		if (p[i] == 10 || p[i] == 13 || p[i] == 0)
			break;
	}
	p[i] = 0;
	parms_in = au (parms_p);
	ret = cfgfile_modify (index, parms_in, size, out_p, outsize);
	xfree (parms_in);
	if (out) {
		parms_out = ua (out_p);
		p = parms_out;
		for (i = 0; i < outsize - 1; i++) {
			uae_u8 b = *p++;
			put_byte (out + i, b);
			put_byte (out + i + 1, 0);
			if (!b)
				break;
		}
	}
	xfree (parms_out);
end:
	xfree (out_p);
	xfree (parms_p);
	return ret;
}

static const TCHAR *cfgfile_read_config_value (const TCHAR *option)
{
	struct strlist *sl;
	for (sl = currprefs.all_lines; sl; sl = sl->next) {
		if (sl->option && !strcasecmp (sl->option, option))
			return sl->value;
	}
	return NULL;
}

uae_u32 cfgfile_uaelib (int mode, uae_u32 name, uae_u32 dst, uae_u32 maxlen)
{
	TCHAR tmp[CONFIG_BLEN];
	int i;

	if (mode)
		return 0;

	for (i = 0; i < sizeof (tmp) / sizeof (TCHAR); i++) {
		tmp[i] = get_byte (name + i);
		if (tmp[i] == 0)
			break;
	}
	tmp[sizeof(tmp) / sizeof (TCHAR) - 1] = 0;
	if (tmp[0] == 0)
		return 0;
	const TCHAR *value = cfgfile_read_config_value (tmp);
	if (value) {
		char *s = ua (value);
		for (i = 0; i < maxlen; i++) {
			put_byte (dst + i, s[i]);
			if (s[i] == 0)
				break;
		}
		xfree (s);
		return dst;
	}
	return 0;
}

uae_u8 *restore_configuration (uae_u8 *src)
{
	TCHAR *s = au ((char*)src);
	//write_log (s);
	xfree (s);
	src += strlen ((char*)src) + 1;
	return src;
}

uae_u8 *save_configuration (int *len, bool fullconfig)
{
	int tmpsize = 100000;
	uae_u8 *dstbak, *dst, *p;
	int index = -1;

	dstbak = dst = xcalloc (uae_u8, tmpsize);
	p = dst;
	for (;;) {
		TCHAR tmpout[1000];
		int ret;
		tmpout[0] = 0;
		ret = cfgfile_modify (index, _T("*"), 1, tmpout, sizeof (tmpout) / sizeof (TCHAR));
		index++;
		if (_tcslen (tmpout) > 0) {
			char *out;
			if (!fullconfig && !_tcsncmp (tmpout, _T("input."), 6))
				continue;
			//write_log (_T("'%s'\n"), tmpout);
			out = uutf8 (tmpout);
			strcpy ((char*)p, out);
			xfree (out);
			strcat ((char*)p, "\n");
			p += strlen ((char*)p);
			if (p - dstbak >= tmpsize - sizeof (tmpout))
				break;
		}
		if (ret >= 0)
			break;
	}
	*len = p - dstbak + 1;
	return dstbak;
}

#ifdef UAE_MINI
static void default_prefs_mini (struct uae_prefs *p, int type)
{
	_tcscpy (p->description, _T("UAE default A500 configuration"));

	p->nr_floppies = 1;
	p->floppyslots[0].dfxtype = DRV_35_DD;
	p->floppyslots[1].dfxtype = DRV_NONE;
	p->cpu_model = 68000;
	p->address_space_24 = 1;
	p->chipmem_size = 0x00080000;
	p->bogomem_size = 0x00080000;
}
#endif

#include "sounddep/sound.h"

void default_prefs (struct uae_prefs *p, int type)
{
	int i;
	int roms[] = { 6, 7, 8, 9, 10, 14, 5, 4, 3, 2, 1, -1 };
	TCHAR zero = 0;
	struct zfile *f;

	reset_inputdevice_config (p);
	memset (p, 0, sizeof (*p));
	_tcscpy (p->description, _T("UAE default configuration"));
	p->config_hardware_path[0] = 0;
	p->config_host_path[0] = 0;

	p->gfx_scandoubler = false;
	p->start_gui = true;
	p->start_debugger = false;

	p->all_lines = 0;
	/* Note to porters: please don't change any of these options! UAE is supposed
	* to behave identically on all platforms if possible.
	* (TW says: maybe it is time to update default config..) */
	p->illegal_mem = 0;
	p->use_serial = 0;
	p->serial_demand = 0;
	p->serial_hwctsrts = 1;
	p->serial_stopbits = 0;
	p->parallel_demand = 0;
	p->parallel_matrix_emulation = 0;
	p->parallel_postscript_emulation = 0;
	p->parallel_postscript_detection = 0;
	p->parallel_autoflush_time = 5;
	p->ghostscript_parameters[0] = 0;
	p->uae_hide = 0;
	p->uae_hide_autoconfig = false;
	p->z3_mapping_mode = Z3MAPPING_AUTO;

	p->mountitems = 0;
	for (i = 0; i < MOUNT_CONFIG_SIZE; i++) {
		p->mountconfig[i].configoffset = -1;
		p->mountconfig[i].unitnum = -1;
	}

	memset (&p->jports[0], 0, sizeof (struct jport));
	memset (&p->jports[1], 0, sizeof (struct jport));
	memset (&p->jports[2], 0, sizeof (struct jport));
	memset (&p->jports[3], 0, sizeof (struct jport));
	p->jports[0].id = JSEM_MICE;
	p->jports[1].id = JSEM_KBDLAYOUT;
	p->jports[2].id = -1;
	p->jports[3].id = -1;
	p->keyboard_lang = KBD_LANG_US;

	p->produce_sound = 3;
	p->sound_stereo = SND_STEREO;
	p->sound_stereo_separation = 7;
	p->sound_mixed_stereo_delay = 0;
	p->sound_freq = DEFAULT_SOUND_FREQ;
	p->sound_maxbsiz = DEFAULT_SOUND_MAXB;
	p->sound_interpol = 1;
	p->sound_filter = FILTER_SOUND_EMUL;
	p->sound_filter_type = 0;
	p->sound_auto = 1;
	p->sound_cdaudio = false;
	p->sampler_stereo = false;
	p->sampler_buffer = 0;
	p->sampler_freq = 0;

	p->comptrustbyte = 0;
	p->comptrustword = 0;
	p->comptrustlong = 0;
	p->comptrustnaddr= 0;
	p->compnf = 1;
	p->comp_hardflush = 0;
	p->comp_constjump = 1;
	p->comp_oldsegv = 0;
	p->compfpu = 1;
	p->cachesize = 0;
	p->avoid_cmov = 0;
	p->comp_midopt = 0;
	p->comp_lowopt = 0;

	for (i = 0;i < 10; i++)
		p->optcount[i] = -1;
	p->optcount[0] = 4;	/* How often a block has to be executed before it is translated */
	p->optcount[1] = 0;	/* How often to use the naive translation */
	p->optcount[2] = 0;
	p->optcount[3] = 0;
	p->optcount[4] = 0;
	p->optcount[5] = 0;

	p->gfx_framerate = 1;
	p->gfx_autoframerate = 50;
	p->gfx_size_fs.width = 800;
	p->gfx_size_fs.height = 600;
	p->gfx_size_win.width = 720;
	p->gfx_size_win.height = 568;
	for (i = 0; i < 4; i++) {
		p->gfx_size_fs_xtra[i].width = 0;
		p->gfx_size_fs_xtra[i].height = 0;
		p->gfx_size_win_xtra[i].width = 0;
		p->gfx_size_win_xtra[i].height = 0;
	}
	p->gfx_resolution = RES_HIRES;
	p->gfx_vresolution = VRES_DOUBLE;
	p->gfx_iscanlines = 1;
	p->gfx_apmode[0].gfx_fullscreen = GFX_WINDOW;
	p->gfx_apmode[1].gfx_fullscreen = GFX_WINDOW;
	p->gfx_xcenter = 0; p->gfx_ycenter = 0;
	p->gfx_xcenter_pos = -1;
	p->gfx_ycenter_pos = -1;
	p->gfx_xcenter_size = -1;
	p->gfx_ycenter_size = -1;
	p->gfx_max_horizontal = RES_HIRES;
	p->gfx_max_vertical = VRES_DOUBLE;
	p->gfx_autoresolution_minv = 0;
	p->gfx_autoresolution_minh = 0;
	p->color_mode = 2;
	p->gfx_blackerthanblack = 0;
	p->gfx_autoresolution_vga = true;
	p->gfx_apmode[0].gfx_backbuffers = 2;
	p->gfx_apmode[1].gfx_backbuffers = 1;

	p->immediate_blits = 0;
	p->waiting_blits = 0;
	p->collision_level = 2;
	p->leds_on_screen = 0;
	p->leds_on_screen_mask[0] = p->leds_on_screen_mask[1] = (1 << LED_MAX) - 1;
	p->keyboard_leds_in_use = 0;
	p->keyboard_leds[0] = p->keyboard_leds[1] = p->keyboard_leds[2] = 0;
	p->scsi = 0;
	p->uaeserial = 0;
	p->cpu_idle = 0;
	p->turbo_emulation = 0;
	p->headless = 0;
	p->catweasel = 0;
	p->tod_hack = 0;
	p->maprom = 0;
	p->boot_rom = 0;
	p->filesys_no_uaefsdb = 0;
	p->filesys_custom_uaefsdb = 1;
	p->picasso96_nocustom = 1;
	p->cart_internal = 1;
	p->sana2 = 0;
	p->clipboard_sharing = false;
	p->native_code = false;

	p->cs_compatible = 1;
	p->cs_rtc = 2;
	p->cs_df0idhw = 1;
	p->cs_a1000ram = 0;
	p->cs_fatgaryrev = -1;
	p->cs_ramseyrev = -1;
	p->cs_agnusrev = -1;
	p->cs_deniserev = -1;
	p->cs_mbdmac = 0;
	p->cs_cd32c2p = p->cs_cd32cd = p->cs_cd32nvram = p->cs_cd32fmv = false;
	p->cs_cd32nvram_size = 1024;
	p->cs_cdtvcd = p->cs_cdtvram = false;
	p->cs_cdtvcard = 0;
	p->cs_pcmcia = 0;
	p->cs_ksmirror_e0 = 1;
	p->cs_ksmirror_a8 = 0;
	p->cs_ciaoverlay = 1;
	p->cs_ciaatod = 0;
	p->cs_df0idhw = 1;
	p->cs_slowmemisfast = 0;
	p->cs_resetwarning = 1;
	p->cs_ciatodbug = false;

	for (int i = APMODE_NATIVE; i <= APMODE_RTG; i++) {
		struct gfx_filterdata *f = &p->gf[i];
		f->gfx_filter = 0;
		f->gfx_filter_scanlineratio = (1 << 4) | 1;
		for (int j = 0; j <= 2 * MAX_FILTERSHADERS; j++) {
			f->gfx_filtershader[i][0] = 0;
			f->gfx_filtermask[i][0] = 0;
		}
		f->gfx_filter_horiz_zoom_mult = 1.0;
		f->gfx_filter_vert_zoom_mult = 1.0;
		f->gfx_filter_bilinear = 0;
		f->gfx_filter_filtermode = 0;
		f->gfx_filter_keep_aspect = 0;
		f->gfx_filter_autoscale = AUTOSCALE_STATIC_AUTO;
		f->gfx_filter_keep_autoscale_aspect = false;
		f->gfx_filteroverlay_overscan = 0;
	}

	p->rtg_horiz_zoom_mult = 1.0;
	p->rtg_vert_zoom_mult = 1.0;

	_tcscpy (p->floppyslots[0].df, _T("df0.adf"));
	_tcscpy (p->floppyslots[1].df, _T("df1.adf"));
	_tcscpy (p->floppyslots[2].df, _T("df2.adf"));
	_tcscpy (p->floppyslots[3].df, _T("df3.adf"));

	for (int i = 0; i < MAX_LUA_STATES; i++) {
		p->luafiles[i][0] = 0;
	}

	configure_rom (p, roms, 0);
	_tcscpy (p->romextfile, _T(""));
	_tcscpy (p->romextfile2, _T(""));
	p->romextfile2addr = 0;
	_tcscpy (p->flashfile, _T(""));
	_tcscpy (p->cartfile, _T(""));
	_tcscpy (p->rtcfile, _T(""));

	_tcscpy (p->path_rom.path[0], _T("./"));
	_tcscpy (p->path_floppy.path[0], _T("./"));
	_tcscpy (p->path_hardfile.path[0], _T("./"));

	p->prtname[0] = 0;
	p->sername[0] = 0;

	p->fpu_model = 0;
	p->cpu_model = 68000;
	p->m68k_speed_throttle = 0;
	p->cpu_clock_multiplier = 0;
	p->cpu_frequency = 0;
	p->mmu_model = 0;
	p->cpu060_revision = 6;
	p->fpu_revision = 0;
	p->fpu_no_unimplemented = false;
	p->int_no_unimplemented = false;
	p->fpu_strict = 0;
	p->fpu_softfloat = 0;
	p->m68k_speed = 0;
	p->cpu_compatible = 1;
	p->address_space_24 = 1;
	p->cpu_cycle_exact = 0;
	p->blitter_cycle_exact = 0;
	p->chipset_mask = CSMASK_ECS_AGNUS;
	p->genlock = 0;
	p->genlock_image = 0;
	p->genlock_mix = 0;
	p->ntscmode = 0;
	p->filesys_limit = 0;
	p->filesys_max_name = 107;
	p->filesys_max_file_size = 0x7fffffff;

	p->fastmem_size = 0x00000000;
	p->fastmem2_size = 0x00000000;
	p->mem25bit_size = 0x00000000;
	p->mbresmem_low_size = 0x00000000;
	p->mbresmem_high_size = 0x00000000;
	p->z3fastmem_size = 0x00000000;
	p->z3fastmem2_size = 0x00000000;
	p->z3autoconfig_start = 0x10000000;
	p->chipmem_size = 0x00080000;
	p->bogomem_size = 0x00080000;
	p->rtgmem_size = 0x00000000;
	p->rtgmem_type = GFXBOARD_UAE_Z3;
	p->custom_memory_addrs[0] = 0;
	p->custom_memory_sizes[0] = 0;
	p->custom_memory_addrs[1] = 0;
	p->custom_memory_sizes[1] = 0;
	p->fastmem_autoconfig = true;

	p->nr_floppies = 2;
	p->floppy_read_only = false;
	p->floppyslots[0].dfxtype = DRV_35_DD;
	p->floppyslots[1].dfxtype = DRV_35_DD;
	p->floppyslots[2].dfxtype = DRV_NONE;
	p->floppyslots[3].dfxtype = DRV_NONE;
	p->floppy_speed = 100;
	p->floppy_write_length = 0;
	p->floppy_random_bits_min = 1;
	p->floppy_random_bits_max = 3;
	p->dfxclickvolume_disk[0] = 33;
	p->dfxclickvolume_disk[1] = 33;
	p->dfxclickvolume_empty[0] = 33;
	p->dfxclickvolume_empty[1] = 33;
	p->dfxclickchannelmask = 0xffff;
	p->cd_speed = 100;

	p->statecapturebuffersize = 100;
	p->statecapturerate = 5 * 50;
	p->inprec_autoplay = true;

#ifdef UAE_MINI
	default_prefs_mini (p, 0);
#endif

	p->input_tablet = TABLET_OFF;
	p->tablet_library = false;
	p->input_magic_mouse = 0;
	p->input_magic_mouse_cursor = 0;

	inputdevice_default_prefs (p);

	blkdev_default_prefs (p);

	p->cr_selected = -1;
	struct chipset_refresh *cr;
	for (int i = 0; i < MAX_CHIPSET_REFRESH_TOTAL; i++) {
		cr = &p->cr[i];
		cr->index = i;
		cr->rate = -1;
	}
	cr = &p->cr[CHIPSET_REFRESH_PAL];
	cr->index = CHIPSET_REFRESH_PAL;
	cr->horiz = -1;
	cr->vert = -1;
	cr->lace = -1;
	cr->vsync = - 1;
	cr->framelength = -1;
	cr->rate = 50.0;
	cr->ntsc = 0;
	cr->locked = false;
	_tcscpy (cr->label, _T("PAL"));
	cr = &p->cr[CHIPSET_REFRESH_NTSC];
	cr->index = CHIPSET_REFRESH_NTSC;
	cr->horiz = -1;
	cr->vert = -1;
	cr->lace = -1;
	cr->vsync = - 1;
	cr->framelength = -1;
	cr->rate = 60.0;
	cr->ntsc = 1;
	cr->locked = false;
	_tcscpy (cr->label, _T("NTSC"));

	savestate_state = 0;

	target_default_options (p, type);

	zfile_fclose (default_file);
	default_file = NULL;
	f = zfile_fopen_empty (NULL, _T("configstore"));
	if (f) {
		uaeconfig++;
		cfgfile_save_options (f, p, 0);
		uaeconfig--;
		cfg_write (&zero, f);
		default_file = f;
	}
}

static void buildin_default_prefs_68020 (struct uae_prefs *p)
{
	p->cpu_model = 68020;
	p->address_space_24 = 1;
	p->cpu_compatible = 1;
	p->chipset_mask = CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE | CSMASK_AGA;
	p->chipmem_size = 0x200000;
	p->bogomem_size = 0;
	p->m68k_speed = -1;
}

static void buildin_default_host_prefs (struct uae_prefs *p)
{
#if 0
	p->sound_filter = FILTER_SOUND_OFF;
	p->sound_stereo = SND_STEREO;
	p->sound_stereo_separation = 7;
	p->sound_mixed_stereo = 0;
#endif
}

static void buildin_default_prefs (struct uae_prefs *p)
{
	buildin_default_host_prefs (p);

	p->floppyslots[0].dfxtype = DRV_35_DD;
	if (p->nr_floppies != 1 && p->nr_floppies != 2)
		p->nr_floppies = 2;
	p->floppyslots[1].dfxtype = p->nr_floppies >= 2 ? DRV_35_DD : DRV_NONE;
	p->floppyslots[2].dfxtype = DRV_NONE;
	p->floppyslots[3].dfxtype = DRV_NONE;
	p->floppy_speed = 100;

	p->fpu_model = 0;
	p->cpu_model = 68000;
	p->cpu_clock_multiplier = 0;
	p->cpu_frequency = 0;
	p->cpu060_revision = 1;
	p->fpu_revision = -1;
	p->m68k_speed = 0;
	p->cpu_compatible = 1;
	p->address_space_24 = 1;
	p->cpu_cycle_exact = 0;
	p->blitter_cycle_exact = 0;
	p->chipset_mask = CSMASK_ECS_AGNUS;
	p->immediate_blits = 0;
	p->waiting_blits = 0;
	p->collision_level = 2;
	if (p->produce_sound < 1)
		p->produce_sound = 1;
	p->scsi = 0;
	p->uaeserial = 0;
	p->cpu_idle = 0;
	p->turbo_emulation = 0;
	p->catweasel = 0;
	p->tod_hack = 0;
	p->maprom = 0;
	p->cachesize = 0;
	p->socket_emu = 0;
	p->sound_volume_master = 0;
	p->sound_volume_paula = 0;
	p->sound_volume_cd = 0;
	p->clipboard_sharing = false;
	p->ppc_mode = 0;

	p->chipmem_size = 0x00080000;
	p->bogomem_size = 0x00080000;
	p->fastmem_size = 0x00000000;
	p->mem25bit_size = 0x00000000;
	p->mbresmem_low_size = 0x00000000;
	p->mbresmem_high_size = 0x00000000;
	p->z3fastmem_size = 0x00000000;
	p->z3fastmem2_size = 0x00000000;
	p->z3chipmem_size = 0x00000000;
	p->rtgmem_size = 0x00000000;
	p->rtgmem_type = GFXBOARD_UAE_Z3;

	p->cs_rtc = 0;
	p->cs_a1000ram = false;
	p->cs_fatgaryrev = -1;
	p->cs_ramseyrev = -1;
	p->cs_agnusrev = -1;
	p->cs_deniserev = -1;
	p->cs_mbdmac = 0;
	p->cs_cd32c2p = p->cs_cd32cd = p->cs_cd32nvram = p->cs_cd32fmv = false;
	p->cs_cdtvcd = p->cs_cdtvram = p->cs_cdtvcard = false;
	p->cs_ide = 0;
	p->cs_pcmcia = 0;
	p->cs_ksmirror_e0 = 1;
	p->cs_ksmirror_a8 = 0;
	p->cs_ciaoverlay = 1;
	p->cs_ciaatod = 0;
	p->cs_df0idhw = 1;
	p->cs_resetwarning = 0;
	p->cs_ciatodbug = false;
	p->cs_1mchipjumper = false;

	_tcscpy (p->romextfile, _T(""));
	_tcscpy (p->romextfile2, _T(""));
	set_device_rom(p, NULL, ROMTYPE_CPUBOARD, 0);

	p->prtname[0] = 0;
	p->sername[0] = 0;

	p->mountitems = 0;

	target_default_options (p, 1);
}

static void set_68020_compa (struct uae_prefs *p, int compa, int cd32)
{
	switch (compa)
	{
	case 0:
		p->blitter_cycle_exact = 1;
		p->m68k_speed = 0;
		if (p->cpu_model == 68020 && p->cachesize == 0) {
			p->cpu_cycle_exact = 1;
			p->cpu_clock_multiplier = 4 << 8;
		}
	break;
	case 1:
		p->cpu_compatible = true;
		p->m68k_speed = 0;
		break;
	case 2:
		p->cpu_compatible = 0;
		p->m68k_speed = -1;
		p->address_space_24 = 0;
		break;
	case 3:
		p->cpu_compatible = 0;
		p->address_space_24 = 0;
		p->cachesize = 8192;
		break;
	}
}

/* 0: cycle-exact
* 1: more compatible
* 2: no more compatible, no 100% sound
* 3: no more compatible, waiting blits, no 100% sound
*/

static void set_68000_compa (struct uae_prefs *p, int compa)
{
	p->cpu_clock_multiplier = 2 << 8;
	switch (compa)
	{
	case 0:
		p->cpu_cycle_exact = p->blitter_cycle_exact = 1;
		break;
	case 1:
		break;
	case 2:
		p->cpu_compatible = 0;
		break;
	case 3:
		p->produce_sound = 2;
		p->cpu_compatible = 0;
		break;
	}
}

static int bip_a3000 (struct uae_prefs *p, int config, int compa, int romcheck)
{
	int roms[2];

	if (config == 2)
		roms[0] = 61;
	else if (config == 1)
		roms[0] = 71;
	else
		roms[0] = 59;
	roms[1] = -1;
	p->bogomem_size = 0;
	p->chipmem_size = 0x200000;
	p->cpu_model = 68030;
	p->fpu_model = 68882;
	p->fpu_no_unimplemented = true;
	if (compa == 0)
		p->mmu_model = 68030;
	else
		p->cachesize = 8192;
	p->chipset_mask = CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE;
	p->cpu_compatible = p->address_space_24 = 0;
	p->m68k_speed = -1;
	p->immediate_blits = 0;
	p->produce_sound = 2;
	p->floppyslots[0].dfxtype = DRV_35_HD;
	p->floppy_speed = 0;
	p->cpu_idle = 150;
	p->cs_compatible = CP_A3000;
	p->mbresmem_low_size = 8 * 1024 * 1024;
	built_in_chipset_prefs (p);
	p->cs_ciaatod = p->ntscmode ? 2 : 1;
	return configure_rom (p, roms, romcheck);
}
static int bip_a4000 (struct uae_prefs *p, int config, int compa, int romcheck)
{
	int roms[8];

	roms[0] = 16;
	roms[1] = 31;
	roms[2] = 13;
	roms[3] = 12;
	roms[4] = -1;

	p->bogomem_size = 0;
	p->chipmem_size = 0x200000;
	p->mbresmem_low_size = 8 * 1024 * 1024;
	p->cpu_model = 68030;
	p->fpu_model = 68882;
	switch (config)
	{
		case 1:
		p->cpu_model = 68040;
		p->fpu_model = 68040;
		break;
		case 2:
		p->cpu_model = 68060;
		p->fpu_model = 68060;
		p->ppc_mode = 1;
		cpuboard_setboard(p, BOARD_CYBERSTORM, BOARD_CYBERSTORM_SUB_PPC);
		p->cpuboardmem1_size = 128 * 1024 * 1024;
		int roms_ppc[] = { 98, -1 };
		configure_rom(p, roms_ppc, romcheck);
		break;
	}
	p->chipset_mask = CSMASK_AGA | CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE;
	p->cpu_compatible = p->address_space_24 = 0;
	p->m68k_speed = -1;
	p->immediate_blits = 0;
	p->produce_sound = 2;
	p->cachesize = 8192;
	p->floppyslots[0].dfxtype = DRV_35_HD;
	p->floppyslots[1].dfxtype = DRV_35_HD;
	p->floppy_speed = 0;
	p->cpu_idle = 150;
	p->cs_compatible = CP_A4000;
	built_in_chipset_prefs (p);
	p->cs_ciaatod = p->ntscmode ? 2 : 1;
	return configure_rom (p, roms, romcheck);
}
static int bip_a4000t (struct uae_prefs *p, int config, int compa, int romcheck)
{

	int roms[8];

	roms[0] = 16;
	roms[1] = 31;
	roms[2] = 13;
	roms[3] = -1;

	p->bogomem_size = 0;
	p->chipmem_size = 0x200000;
	p->mbresmem_low_size = 8 * 1024 * 1024;
	p->cpu_model = 68030;
	p->fpu_model = 68882;
	if (config > 0) {
		p->cpu_model = 68040;
		p->fpu_model = 68040;
	}
	p->chipset_mask = CSMASK_AGA | CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE;
	p->cpu_compatible = p->address_space_24 = 0;
	p->m68k_speed = -1;
	p->immediate_blits = 0;
	p->produce_sound = 2;
	p->cachesize = 8192;
	p->floppyslots[0].dfxtype = DRV_35_HD;
	p->floppyslots[1].dfxtype = DRV_35_HD;
	p->floppy_speed = 0;
	p->cpu_idle = 150;
	p->cs_compatible = CP_A4000T;
	built_in_chipset_prefs (p);
	p->cs_ciaatod = p->ntscmode ? 2 : 1;
	return configure_rom (p, roms, romcheck);
}

static void bip_velvet(struct uae_prefs *p, int config, int compa, int romcheck)
{
	p->chipset_mask = 0;
	p->bogomem_size = 0;
	p->sound_filter = FILTER_SOUND_ON;
	set_68000_compa (p, compa);
	p->floppyslots[1].dfxtype = DRV_NONE;
	p->cs_compatible = CP_VELVET;
	p->cs_slowmemisfast = 1;
	p->cs_dipagnus = 1;
	p->cs_agnusbltbusybug = 1;
	built_in_chipset_prefs (p);
	p->cs_denisenoehb = 1;
	p->cs_cia6526 = 1;
	p->chipmem_size = 0x40000;
}

static int bip_a1000 (struct uae_prefs *p, int config, int compa, int romcheck)
{
	int roms[2];

	roms[0] = 24;
	roms[1] = -1;
	p->chipset_mask = 0;
	p->bogomem_size = 0;
	p->sound_filter = FILTER_SOUND_ON;
	set_68000_compa (p, compa);
	p->floppyslots[1].dfxtype = DRV_NONE;
	p->cs_compatible = CP_A1000;
	p->cs_slowmemisfast = 1;
	p->cs_dipagnus = 1;
	p->cs_agnusbltbusybug = 1;
	built_in_chipset_prefs (p);
	if (config > 0)
		p->cs_denisenoehb = 1;
	if (config > 1)
		p->chipmem_size = 0x40000;
	if (config > 2) {
		roms[0] = 125;
		roms[1] = -1;
		bip_velvet(p, config, compa, romcheck);
	}
	return configure_rom (p, roms, romcheck);
}

static int bip_cdtvcr (struct uae_prefs *p, int config, int compa, int romcheck)
{
	int roms[4];

	p->bogomem_size = 0;
	p->chipmem_size = 0x100000;
	p->chipset_mask = CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE;
	p->cs_cdtvcd = p->cs_cdtvram = true;
	p->cs_cdtvcr = true;
	p->cs_rtc = 1;
	p->nr_floppies = 0;
	p->floppyslots[0].dfxtype = DRV_NONE;
	if (config > 0)
		p->floppyslots[0].dfxtype = DRV_35_DD;
	p->floppyslots[1].dfxtype = DRV_NONE;
	set_68000_compa (p, compa);
	p->cs_compatible = CP_CDTVCR;
	built_in_chipset_prefs (p);
	fetch_datapath (p->flashfile, sizeof (p->flashfile) / sizeof (TCHAR));
	_tcscat (p->flashfile, _T("cdtv-cr.nvr"));
	roms[0] = 9;
	roms[1] = 10;
	roms[2] = -1;
	if (!configure_rom (p, roms, romcheck))
		return 0;
	roms[0] = 108;
	roms[1] = 107;
	roms[2] = -1;
	if (!configure_rom (p, roms, romcheck))
		return 0;
	return 1;
}

static int bip_cdtv (struct uae_prefs *p, int config, int compa, int romcheck)
{
	int roms[4];

	if (config >= 2)
		return bip_cdtvcr(p, config - 2, compa, romcheck);

	p->bogomem_size = 0;
	p->chipmem_size = 0x100000;
	p->chipset_mask = CSMASK_ECS_AGNUS;
	p->cs_cdtvcd = p->cs_cdtvram = 1;
	if (config > 0)
		p->cs_cdtvcard = 64;
	p->cs_rtc = 1;
	p->nr_floppies = 0;
	p->floppyslots[0].dfxtype = DRV_NONE;
	if (config > 0)
		p->floppyslots[0].dfxtype = DRV_35_DD;
	p->floppyslots[1].dfxtype = DRV_NONE;
	set_68000_compa (p, compa);
	p->cs_compatible = CP_CDTV;
	built_in_chipset_prefs (p);
	fetch_datapath (p->flashfile, sizeof (p->flashfile) / sizeof (TCHAR));
	_tcscat (p->flashfile, _T("cdtv.nvr"));
	roms[0] = 6;
	roms[1] = 32;
	roms[2] = -1;
	if (!configure_rom (p, roms, romcheck))
		return 0;
	roms[0] = 20;
	roms[1] = 21;
	roms[2] = 22;
	roms[3] = -1;
	if (!configure_rom (p, roms, romcheck))
		return 0;
	return 1;
}

static int bip_cd32 (struct uae_prefs *p, int config, int compa, int romcheck)
{
	int roms[3];

	buildin_default_prefs_68020 (p);
	p->cs_cd32c2p = p->cs_cd32cd = p->cs_cd32nvram = true;
	p->nr_floppies = 0;
	p->floppyslots[0].dfxtype = DRV_NONE;
	p->floppyslots[1].dfxtype = DRV_NONE;
	set_68020_compa (p, compa, 1);
	p->cs_compatible = CP_CD32;
	built_in_chipset_prefs (p);
	fetch_datapath (p->flashfile, sizeof (p->flashfile) / sizeof (TCHAR));
	_tcscat (p->flashfile, _T("cd32.nvr"));
	roms[0] = 64;
	roms[1] = -1;
	if (!configure_rom (p, roms, 0)) {
		roms[0] = 18;
		roms[1] = -1;
		if (!configure_rom (p, roms, romcheck))
			return 0;
		roms[0] = 19;
		if (!configure_rom (p, roms, romcheck))
			return 0;
	}
	if (config > 0) {
		p->cs_cd32fmv = true;
		roms[0] = 74;
		roms[1] = 23;
		roms[2] = -1;
		if (!configure_rom (p, roms, romcheck))
			return 0;
	}
	return 1;
}

static int bip_a1200 (struct uae_prefs *p, int config, int compa, int romcheck)
{
	int roms[4];
	int roms_bliz[2];

	buildin_default_prefs_68020 (p);
	roms[0] = 11;
	roms[1] = 15;
	roms[2] = 31;
	roms[3] = -1;
	roms_bliz[0] = -1;
	roms_bliz[1] = -1;
	p->cs_rtc = 0;
	p->cs_compatible = CP_A1200;
	built_in_chipset_prefs (p);
	switch (config)
	{
		case 1:
		p->fastmem_size = 0x400000;
		p->cs_rtc = 1;
		break;
		case 2:
		cpuboard_setboard(p, BOARD_BLIZZARD, BOARD_BLIZZARD_SUB_1230IV);
		p->cpuboardmem1_size = 32 * 1024 * 1024;
		p->cpu_model = 68030;
		p->cs_rtc = 1;
		roms_bliz[0] = 89;
		configure_rom(p, roms_bliz, romcheck);
		break;
		case 3:
		cpuboard_setboard(p, BOARD_BLIZZARD, BOARD_BLIZZARD_SUB_1260);
		p->cpuboardmem1_size = 32 * 1024 * 1024;
		p->cpu_model = 68040;
		p->fpu_model = 68040;
		p->cs_rtc = 1;
		roms_bliz[0] = 90;
		configure_rom(p, roms_bliz, romcheck);
		break;
		case 4:
		cpuboard_setboard(p, BOARD_BLIZZARD, BOARD_BLIZZARD_SUB_1260);
		p->cpuboardmem1_size = 32 * 1024 * 1024;
		p->cpu_model = 68060;
		p->fpu_model = 68060;
		p->cs_rtc = 1;
		roms_bliz[0] = 90;
		configure_rom(p, roms_bliz, romcheck);
		break;
		case 5:
		cpuboard_setboard(p, BOARD_BLIZZARD, BOARD_BLIZZARD_SUB_PPC);
		p->cpuboardmem1_size = 256 * 1024 * 1024;
		p->cpu_model = 68060;
		p->fpu_model = 68060;
		p->ppc_mode = 1;
		p->cs_rtc = 1;
		roms[0] = 15;
		roms[1] = 11;
		roms[2] = -1;
		roms_bliz[0] = 100;
		configure_rom(p, roms_bliz, romcheck);
		break;
	}
	set_68020_compa (p, compa, 0);
	return configure_rom (p, roms, romcheck);
}

static int bip_a600 (struct uae_prefs *p, int config, int compa, int romcheck)
{
	int roms[4];

	roms[0] = 10;
	roms[1] = 9;
	roms[2] = 8;
	roms[3] = -1;
	set_68000_compa (p, compa);
	p->cs_compatible = CP_A600;
	built_in_chipset_prefs (p);
	p->bogomem_size = 0;
	p->chipmem_size = 0x100000;
	if (config > 0)
		p->cs_rtc = 1;
	if (config == 1)
		p->chipmem_size = 0x200000;
	if (config == 2)
		p->fastmem_size = 0x400000;
	p->chipset_mask = CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE;
	return configure_rom (p, roms, romcheck);
}

static int bip_a500p (struct uae_prefs *p, int config, int compa, int romcheck)
{
	int roms[2];

	roms[0] = 7;
	roms[1] = -1;
	set_68000_compa (p, compa);
	p->cs_compatible = CP_A500P;
	built_in_chipset_prefs (p);
	p->bogomem_size = 0;
	p->chipmem_size = 0x100000;
	if (config > 0)
		p->cs_rtc = 1;
	if (config == 1)
		p->chipmem_size = 0x200000;
	if (config == 2)
		p->fastmem_size = 0x400000;
	p->chipset_mask = CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE;
	return configure_rom (p, roms, romcheck);
}
static int bip_a500 (struct uae_prefs *p, int config, int compa, int romcheck)
{
	int roms[4];

	roms[0] = roms[1] = roms[2] = roms[3] = -1;
	switch (config)
	{
	case 0: // KS 1.3, OCS Agnus, 0.5M Chip + 0.5M Slow
		roms[0] = 6;
		roms[1] = 32;
		p->chipset_mask = 0;
		break;
	case 1: // KS 1.3, ECS Agnus, 0.5M Chip + 0.5M Slow
		roms[0] = 6;
		roms[1] = 32;
		break;
	case 2: // KS 1.3, ECS Agnus, 1.0M Chip
		roms[0] = 6;
		roms[1] = 32;
		p->bogomem_size = 0;
		p->chipmem_size = 0x100000;
		break;
	case 3: // KS 1.3, OCS Agnus, 0.5M Chip
		roms[0] = 6;
		roms[1] = 32;
		p->bogomem_size = 0;
		p->chipset_mask = 0;
		p->cs_rtc = 0;
		p->floppyslots[1].dfxtype = DRV_NONE;
		break;
	case 4: // KS 1.2, OCS Agnus, 0.5M Chip
		roms[0] = 5;
		roms[1] = 4;
		roms[2] = 3;
		p->bogomem_size = 0;
		p->chipset_mask = 0;
		p->cs_rtc = 0;
		p->floppyslots[1].dfxtype = DRV_NONE;
		break;
	case 5: // KS 1.2, OCS Agnus, 0.5M Chip + 0.5M Slow
		roms[0] = 5;
		roms[1] = 4;
		roms[2] = 3;
		p->chipset_mask = 0;
		break;
	}
	set_68000_compa (p, compa);
	p->cs_compatible = CP_A500;
	built_in_chipset_prefs (p);
	return configure_rom (p, roms, romcheck);
}

static int bip_super (struct uae_prefs *p, int config, int compa, int romcheck)
{
	int roms[8];

	roms[0] = 46;
	roms[1] = 16;
	roms[2] = 31;
	roms[3] = 15;
	roms[4] = 14;
	roms[5] = 12;
	roms[6] = 11;
	roms[7] = -1;
	p->bogomem_size = 0;
	p->chipmem_size = 0x400000;
	p->z3fastmem_size = 8 * 1024 * 1024;
	p->rtgmem_size = 16 * 1024 * 1024;
	p->cpu_model = 68040;
	p->fpu_model = 68040;
	p->chipset_mask = CSMASK_AGA | CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE;
	p->cpu_compatible = p->address_space_24 = 0;
	p->m68k_speed = -1;
	p->immediate_blits = 1;
	p->produce_sound = 2;
	p->cachesize = 8192;
	p->floppyslots[0].dfxtype = DRV_35_HD;
	p->floppyslots[1].dfxtype = DRV_35_HD;
	p->floppy_speed = 0;
	p->cpu_idle = 150;
	p->scsi = 1;
	p->uaeserial = 1;
	p->socket_emu = 1;
	p->cart_internal = 0;
	p->picasso96_nocustom = 1;
	p->cs_compatible = 1;
	built_in_chipset_prefs (p);
	p->cs_ide = -1;
	p->cs_ciaatod = p->ntscmode ? 2 : 1;
	//_tcscat(p->flashfile, _T("battclock.nvr"));
	return configure_rom (p, roms, romcheck);
}

static int bip_arcadia (struct uae_prefs *p, int config, int compa, int romcheck)
{
	int roms[4], i;
	struct romlist **rl;

	p->bogomem_size = 0;
	p->chipset_mask = 0;
	p->cs_rtc = 0;
	p->nr_floppies = 0;
	p->floppyslots[0].dfxtype = DRV_NONE;
	p->floppyslots[1].dfxtype = DRV_NONE;
	set_68000_compa (p, compa);
	p->cs_compatible = CP_A500;
	built_in_chipset_prefs (p);
	fetch_datapath (p->flashfile, sizeof (p->flashfile) / sizeof (TCHAR));
	_tcscat (p->flashfile, _T("arcadia.nvr"));
	roms[0] = 5;
	roms[1] = 4;
	roms[2] = -1;
	if (!configure_rom (p, roms, romcheck))
		return 0;
	roms[0] = 51;
	roms[1] = 49;
	roms[2] = -1;
	if (!configure_rom (p, roms, romcheck))
		return 0;
	rl = getarcadiaroms ();
	for (i = 0; rl[i]; i++) {
		if (config-- == 0) {
			roms[0] = rl[i]->rd->id;
			roms[1] = -1;
			configure_rom (p, roms, 0);
			break;
		}
	}
	xfree (rl);
	return 1;
}

int built_in_prefs (struct uae_prefs *p, int model, int config, int compa, int romcheck)
{
	int v = 0;

	buildin_default_prefs (p);
	switch (model)
	{
	case 0:
		v = bip_a500 (p, config, compa, romcheck);
		break;
	case 1:
		v = bip_a500p (p, config, compa, romcheck);
		break;
	case 2:
		v = bip_a600 (p, config, compa, romcheck);
		break;
	case 3:
		v = bip_a1000 (p, config, compa, romcheck);
		break;
	case 4:
		v = bip_a1200 (p, config, compa, romcheck);
		break;
	case 5:
		v = bip_a3000 (p, config, compa, romcheck);
		break;
	case 6:
		v = bip_a4000 (p, config, compa, romcheck);
		break;
	case 7:
		v = bip_a4000t (p, config, compa, romcheck);
		break;
	case 8:
		v = bip_cd32 (p, config, compa, romcheck);
		break;
	case 9:
		v = bip_cdtv (p, config, compa, romcheck);
		break;
	case 10:
		v = bip_arcadia (p, config , compa, romcheck);
		break;
	case 11:
		v = bip_super (p, config, compa, romcheck);
		break;
	}
	if ((p->cpu_model >= 68020 || !p->cpu_cycle_exact) && !p->immediate_blits)
		p->waiting_blits = 1;
	if (p->sound_filter_type == FILTER_SOUND_TYPE_A500 && (p->chipset_mask & CSMASK_AGA))
		p->sound_filter_type = FILTER_SOUND_TYPE_A1200;
	else if (p->sound_filter_type == FILTER_SOUND_TYPE_A1200 && !(p->chipset_mask & CSMASK_AGA))
		p->sound_filter_type = FILTER_SOUND_TYPE_A500;
	return v;
}

int built_in_chipset_prefs (struct uae_prefs *p)
{
	if (!p->cs_compatible)
		return 1;

	p->cs_a1000ram = 0;
	p->cs_cd32c2p = p->cs_cd32cd = p->cs_cd32nvram = 0;
	p->cs_cdtvcd = p->cs_cdtvram = p->cs_cdtvscsi = p->cs_cdtvcr = 0;
	p->cs_fatgaryrev = -1;
	p->cs_ide = 0;
	p->cs_ramseyrev = -1;
	p->cs_deniserev = -1;
	p->cs_agnusrev = -1;
	p->cs_denisenoehb = 0;
	p->cs_dipagnus = 0;
	p->cs_agnusbltbusybug = 0;
	p->cs_mbdmac = 0;
	p->cs_pcmcia = 0;
	p->cs_ksmirror_e0 = 1;
	p->cs_ksmirror_a8 = 0;
	p->cs_ciaoverlay = 1;
	p->cs_ciaatod = 0;
	p->cs_rtc = 0;
	p->cs_rtc_adjust_mode = p->cs_rtc_adjust = 0;
	p->cs_df0idhw = 1;
	p->cs_resetwarning = 1;
	p->cs_slowmemisfast = 0;
	p->cs_ciatodbug = false;
	p->cs_z3autoconfig = false;

	switch (p->cs_compatible)
	{
	case CP_GENERIC: // generic
		p->cs_rtc = 2;
		p->cs_fatgaryrev = 0;
		p->cs_ide = -1;
		p->cs_mbdmac = -1;
		p->cs_ramseyrev = 0x0f;
		break;
	case CP_CDTV: // CDTV
		p->cs_rtc = 1;
		p->cs_cdtvcd = p->cs_cdtvram = 1;
		p->cs_df0idhw = 1;
		p->cs_ksmirror_e0 = 0;
		break;
	case CP_CDTVCR: // CDTV-CR
		p->cs_rtc = 1;
		p->cs_cdtvcd = p->cs_cdtvram = 1;
		p->cs_cdtvcr = true;
		p->cs_df0idhw = 1;
		p->cs_ksmirror_e0 = 0;
		p->cs_ide = IDE_A600A1200;
		p->cs_pcmcia = 1;
		p->cs_ksmirror_a8 = 1;
		p->cs_ciaoverlay = 0;
		p->cs_resetwarning = 0;
		p->cs_ciatodbug = true;
		break;
	case CP_CD32: // CD32
		p->cs_cd32c2p = p->cs_cd32cd = p->cs_cd32nvram = true;
		p->cs_ksmirror_e0 = 0;
		p->cs_ksmirror_a8 = 1;
		p->cs_ciaoverlay = 0;
		p->cs_resetwarning = 0;
		break;
	case CP_A500: // A500
		p->cs_df0idhw = 0;
		p->cs_resetwarning = 0;
		if (p->bogomem_size || p->chipmem_size > 0x80000 || p->fastmem_size)
			p->cs_rtc = 1;
		p->cs_ciatodbug = true;
		break;
	case CP_A500P: // A500+
		p->cs_rtc = 1;
		p->cs_resetwarning = 0;
		p->cs_ciatodbug = true;
		break;
	case CP_A600: // A600
		p->cs_ide = IDE_A600A1200;
		p->cs_pcmcia = 1;
		p->cs_ksmirror_a8 = 1;
		p->cs_ciaoverlay = 0;
		p->cs_resetwarning = 0;
		p->cs_ciatodbug = true;
		break;
	case CP_A1000: // A1000
		p->cs_a1000ram = 1;
		p->cs_ciaatod = p->ntscmode ? 2 : 1;
		p->cs_ksmirror_e0 = 0;
		p->cs_agnusbltbusybug = 1;
		p->cs_dipagnus = 1;
		p->cs_ciatodbug = true;
		break;
	case CP_VELVET: // A1000 Prototype
		p->cs_ciaatod = p->ntscmode ? 2 : 1;
		p->cs_ksmirror_e0 = 0;
		p->cs_agnusbltbusybug = 1;
		p->cs_dipagnus = 1;
		p->cs_denisenoehb = 1;
		break;
	case CP_A1200: // A1200
		p->cs_ide = IDE_A600A1200;
		p->cs_pcmcia = 1;
		p->cs_ksmirror_a8 = 1;
		p->cs_ciaoverlay = 0;
		if (p->fastmem_size || p->z3fastmem_size || p->cpuboard_type)
			p->cs_rtc = 1;
		break;
	case CP_A2000: // A2000
		p->cs_rtc = 1;
		p->cs_ciaatod = p->ntscmode ? 2 : 1;
		p->cs_ciatodbug = true;
		break;
	case CP_A3000: // A3000
		p->cs_rtc = 2;
		p->cs_fatgaryrev = 0;
		p->cs_ramseyrev = 0x0d;
		p->cs_mbdmac = 1;
		p->cs_ksmirror_e0 = 0;
		p->cs_ciaatod = p->ntscmode ? 2 : 1;
		p->cs_z3autoconfig = true;
		break;
	case CP_A3000T: // A3000T
		p->cs_rtc = 2;
		p->cs_fatgaryrev = 0;
		p->cs_ramseyrev = 0x0d;
		p->cs_mbdmac = 1;
		p->cs_ksmirror_e0 = 0;
		p->cs_ciaatod = p->ntscmode ? 2 : 1;
		p->cs_z3autoconfig = true;
		break;
	case CP_A4000: // A4000
		p->cs_rtc = 2;
		p->cs_fatgaryrev = 0;
		p->cs_ramseyrev = 0x0f;
		p->cs_ide = IDE_A4000;
		p->cs_mbdmac = 0;
		p->cs_ksmirror_a8 = 0;
		p->cs_ksmirror_e0 = 0;
		p->cs_ciaoverlay = 0;
		p->cs_z3autoconfig = true;
		break;
	case CP_A4000T: // A4000T
		p->cs_rtc = 2;
		p->cs_fatgaryrev = 0;
		p->cs_ramseyrev = 0x0f;
		p->cs_ide = IDE_A4000;
		p->cs_mbdmac = 2;
		p->cs_ksmirror_a8 = 0;
		p->cs_ksmirror_e0 = 0;
		p->cs_ciaoverlay = 0;
		p->cs_z3autoconfig = true;
		break;
	}
	return 1;
}

int built_in_cpuboard_prefs(struct uae_prefs *p)
{
	int roms[2], roms2[2];
	
	roms[0] = -1;
	roms[1] = -1;
	roms2[0] = -1;
	roms2[1] = -1;

	switch(cpuboards[p->cpuboard_type].id)
	{
		case BOARD_MACROSYSTEM:
		switch(p->cpuboard_subtype)
		{
			case BOARD_MACROSYSTEM_SUB_WARPENGINE_A4000:
			roms[0] = 93;
			break;
		}
		break;
		case BOARD_BLIZZARD:
		switch(p->cpuboard_subtype)
		{
		case BOARD_BLIZZARD_SUB_1230IV:
			roms[0] = 89;
			break;
		case BOARD_BLIZZARD_SUB_1260:
			roms[0] = 90;
			break;
		case BOARD_BLIZZARD_SUB_2060:
			roms[0] = 92;
			break;
		case BOARD_BLIZZARD_SUB_PPC:
			roms[0] = p->cpu_model == 68040 ? 99 : 100;
			break;
		}
		break;
		case BOARD_CYBERSTORM:
		switch(p->cpuboard_subtype)
		{
		case BOARD_CYBERSTORM_SUB_MK1:
			roms[0] = p->cpu_model == 68040 ? 95 : 101;
			break;
		case BOARD_CYBERSTORM_SUB_MK2:
			roms[0] = 96;
			break;
		case BOARD_CYBERSTORM_SUB_MK3:
			roms[0] = 97;
			break;
		case BOARD_CYBERSTORM_SUB_PPC:
			roms[0] = 98;
			break;
		}
		break;
	}
	if (!configure_rom(p, roms, 0))
		return 0;
	if (!configure_rom(p, roms2, 0))
		return 0;
	return 1;
}

void set_config_changed (void)
{
	config_changed = 1;
}

void config_check_vsync (void)
{
	if (config_changed) {
#ifdef WITH_LUA
		if (config_changed == 1) {
			createconfigstore (&currprefs);
			uae_lua_run_handler ("on_uae_config_changed");
		}
#endif
		config_changed++;
		if (config_changed >= 3)
			config_changed = 0;
	}
}

bool is_error_log (void)
{
	return error_lines != NULL;
}
TCHAR *get_error_log (void)
{
	strlist *sl;
	int len = 0;
	for (sl = error_lines; sl; sl = sl->next) {
		len += _tcslen (sl->option) + 1;
	}
	if (!len)
		return NULL;
	TCHAR *s = xcalloc (TCHAR, len + 1);
	for (sl = error_lines; sl; sl = sl->next) {
		_tcscat (s, sl->option);
		_tcscat (s, _T("\n"));
	}
	return s;
}
void error_log (const TCHAR *format, ...)
{
	TCHAR buffer[256], *bufp;
	int bufsize = 256;
	va_list parms;

	if (format == NULL) {
		struct strlist **ps = &error_lines;
		while (*ps) {
			struct strlist *s = *ps;
			*ps = s->next;
			xfree (s->value);
			xfree (s->option);
			xfree (s);
		}
		return;
	}

	va_start (parms, format);
	bufp = buffer;
	for (;;) {
		int count = _vsntprintf (bufp, bufsize - 1, format, parms);
		if (count < 0) {
			bufsize *= 10;
			if (bufp != buffer)
				xfree (bufp);
			bufp = xmalloc (TCHAR, bufsize);
			continue;
		}
		break;
	}
	bufp[bufsize - 1] = 0;
	write_log (_T("%s\n"), bufp);
	va_end (parms);

	strlist *u = xcalloc (struct strlist, 1);
	u->option = my_strdup (bufp);
	u->next = error_lines;
	error_lines = u;

	if (bufp != buffer)
		xfree (bufp);
}
