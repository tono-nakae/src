/*	$NetBSD: component.c,v 1.1 2010/02/11 02:22:10 pooka Exp $	*/

/*
 * MACHINE GENERATED: DO NOT EDIT
 *
 * ioconf.c, from "TESTI_wscons"
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/mount.h>

static const struct cfiattrdata gpibdevcf_iattrdata = {
	"gpibdev", 1,
	{
		{ "address", "-1", -1 },
	}
};
static const struct cfiattrdata acpibuscf_iattrdata = {
	"acpibus", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata caccf_iattrdata = {
	"cac", 1,
	{
		{ "unit", "-1", -1 },
	}
};
static const struct cfiattrdata spicf_iattrdata = {
	"spi", 1,
	{
		{ "slave", "NULL", 0 },
	}
};
static const struct cfiattrdata radiodevcf_iattrdata = {
	"radiodev", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata mlxcf_iattrdata = {
	"mlx", 1,
	{
		{ "unit", "-1", -1 },
	}
};
static const struct cfiattrdata scsibuscf_iattrdata = {
	"scsibus", 2,
	{
		{ "target", "-1", -1 },
		{ "lun", "-1", -1 },
	}
};
static const struct cfiattrdata ucombuscf_iattrdata = {
	"ucombus", 1,
	{
		{ "portno", "-1", -1 },
	}
};
static const struct cfiattrdata videobuscf_iattrdata = {
	"videobus", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata isabuscf_iattrdata = {
	"isabus", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata i2cbuscf_iattrdata = {
	"i2cbus", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata ata_hlcf_iattrdata = {
	"ata_hl", 1,
	{
		{ "drive", "-1", -1 },
	}
};
static const struct cfiattrdata mainbuscf_iattrdata = {
	"mainbus", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata depcacf_iattrdata = {
	"depca", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata ppbuscf_iattrdata = {
	"ppbus", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata eisabuscf_iattrdata = {
	"eisabus", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata atapicf_iattrdata = {
	"atapi", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata atapibuscf_iattrdata = {
	"atapibus", 1,
	{
		{ "drive", "-1", -1 },
	}
};
static const struct cfiattrdata usbroothubifcf_iattrdata = {
	"usbroothubif", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata altmemdevcf_iattrdata = {
	"altmemdev", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata tcbuscf_iattrdata = {
	"tcbus", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata onewirebuscf_iattrdata = {
	"onewirebus", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata gpiocf_iattrdata = {
	"gpio", 2,
	{
		{ "offset", "-1", -1 },
		{ "mask", "0", 0 },
	}
};
static const struct cfiattrdata cbbuscf_iattrdata = {
	"cbbus", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata gpiobuscf_iattrdata = {
	"gpiobus", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata drmcf_iattrdata = {
	"drm", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata pckbportcf_iattrdata = {
	"pckbport", 1,
	{
		{ "slot", "-1", -1 },
	}
};
static const struct cfiattrdata irbuscf_iattrdata = {
	"irbus", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata aaccf_iattrdata = {
	"aac", 1,
	{
		{ "unit", "-1", -1 },
	}
};
static const struct cfiattrdata pcibuscf_iattrdata = {
	"pcibus", 1,
	{
		{ "bus", "-1", -1 },
	}
};
static const struct cfiattrdata usbififcf_iattrdata = {
	"usbifif", 6,
	{
		{ "port", "-1", -1 },
		{ "configuration", "-1", -1 },
		{ "interface", "-1", -1 },
		{ "vendor", "-1", -1 },
		{ "product", "-1", -1 },
		{ "release", "-1", -1 },
	}
};
static const struct cfiattrdata upccf_iattrdata = {
	"upc", 1,
	{
		{ "offset", "-1", -1 },
	}
};
static const struct cfiattrdata iiccf_iattrdata = {
	"iic", 2,
	{
		{ "addr", "-1", -1 },
		{ "size", "-1", -1 },
	}
};
static const struct cfiattrdata onewirecf_iattrdata = {
	"onewire", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata mcabuscf_iattrdata = {
	"mcabus", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata wsdisplaydevcf_iattrdata = {
	"wsdisplaydev", 1,
	{
		{ "kbdmux", "1", 1 },
	}
};
static const struct cfiattrdata miicf_iattrdata = {
	"mii", 1,
	{
		{ "phy", "-1", -1 },
	}
};
static const struct cfiattrdata cpcbuscf_iattrdata = {
	"cpcbus", 2,
	{
		{ "addr", "NULL", 0 },
		{ "irq", "-1", -1 },
	}
};
static const struct cfiattrdata parportcf_iattrdata = {
	"parport", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata dbcoolcf_iattrdata = {
	"dbcool", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata usbdevifcf_iattrdata = {
	"usbdevif", 6,
	{
		{ "port", "-1", -1 },
		{ "configuration", "-1", -1 },
		{ "interface", "-1", -1 },
		{ "vendor", "-1", -1 },
		{ "product", "-1", -1 },
		{ "release", "-1", -1 },
	}
};
static const struct cfiattrdata wskbddevcf_iattrdata = {
	"wskbddev", 2,
	{
		{ "console", "-1", -1 },
		{ "mux", "1", 1 },
	}
};
static const struct cfiattrdata audiobuscf_iattrdata = {
	"audiobus", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata btbuscf_iattrdata = {
	"btbus", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata midibuscf_iattrdata = {
	"midibus", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata vmebuscf_iattrdata = {
	"vmebus", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata wsemuldisplaydevcf_iattrdata = {
	"wsemuldisplaydev", 2,
	{
		{ "console", "-1", -1 },
		{ "kbdmux", "1", 1 },
	}
};
static const struct cfiattrdata uhidbuscf_iattrdata = {
	"uhidbus", 1,
	{
		{ "reportid", "-1", -1 },
	}
};
static const struct cfiattrdata icpcf_iattrdata = {
	"icp", 1,
	{
		{ "unit", "-1", -1 },
	}
};
static const struct cfiattrdata sdmmcbuscf_iattrdata = {
	"sdmmcbus", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata comcf_iattrdata = {
	"com", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata spiflashbuscf_iattrdata = {
	"spiflashbus", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata fwbuscf_iattrdata = {
	"fwbus", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata pcmciaslotcf_iattrdata = {
	"pcmciaslot", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata usbuscf_iattrdata = {
	"usbus", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata wsmousedevcf_iattrdata = {
	"wsmousedev", 1,
	{
		{ "mux", "0", 0 },
	}
};
static const struct cfiattrdata scsicf_iattrdata = {
	"scsi", 1,
	{
		{ "channel", "-1", -1 },
	}
};
static const struct cfiattrdata atacf_iattrdata = {
	"ata", 1,
	{
		{ "channel", "-1", -1 },
	}
};
static const struct cfiattrdata spibuscf_iattrdata = {
	"spibus", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata pcmciabuscf_iattrdata = {
	"pcmciabus", 2,
	{
		{ "controller", "-1", -1 },
		{ "socket", "-1", -1 },
	}
};

CFDRIVER_DECL(wskbd, DV_DULL, NULL);

CFDRIVER_DECL(wsmouse, DV_DULL, NULL);

static const struct cfiattrdata * const usb_attrs[] = { &usbroothubifcf_iattrdata, NULL };
CFDRIVER_DECL(usb, DV_DULL, usb_attrs);

static const struct cfiattrdata * const uhub_attrs[] = { &usbififcf_iattrdata, &usbdevifcf_iattrdata, NULL };
CFDRIVER_DECL(uhub, DV_DULL, uhub_attrs);

static const struct cfiattrdata * const uhidev_attrs[] = { &uhidbuscf_iattrdata, NULL };
CFDRIVER_DECL(uhidev, DV_DULL, uhidev_attrs);

CFDRIVER_DECL(uhid, DV_DULL, NULL);

static const struct cfiattrdata * const ukbd_attrs[] = { &wskbddevcf_iattrdata, NULL };
CFDRIVER_DECL(ukbd, DV_DULL, ukbd_attrs);

static const struct cfiattrdata * const ums_attrs[] = { &wsmousedevcf_iattrdata, NULL };
CFDRIVER_DECL(ums, DV_DULL, ums_attrs);

static const struct cfiattrdata * const mainbus_attrs[] = { &mainbuscf_iattrdata, NULL };
CFDRIVER_DECL(mainbus, DV_DULL, mainbus_attrs);



extern struct cfattach wskbd_ca;
extern struct cfattach wsmouse_ca;
extern struct cfattach usb_ca;
extern struct cfattach uroothub_ca;
extern struct cfattach uhub_ca;
extern struct cfattach uhidev_ca;
extern struct cfattach uhid_ca;
extern struct cfattach ukbd_ca;
extern struct cfattach ums_ca;
extern struct cfattach mainbus_ca;

/* locators */
static int loc[18] = {
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, 1, -1, -1,
	0, -1,
};

static const struct cfparent pspec6 = {
	"wskbddev", "ukbd", DVUNIT_ANY
};
static const struct cfparent pspec7 = {
	"wsmousedev", "ums", DVUNIT_ANY
};

#define NORM FSTATE_NOTFOUND
#define STAR FSTATE_STAR

struct cfdata cfdata_wscons[] = {
    /* driver           attachment    unit state loc   flags pspec */
/*  0: wskbd* at ukbd? console -1 mux 1 */
    { "wskbd",		"wskbd",	 0, STAR, loc+ 12,      0, &pspec6 },
/*  1: wsmouse* at ums? mux 0 */
    { "wsmouse",		"wsmouse",	 0, STAR, loc+ 16,      0, &pspec7 },
    { NULL,		NULL,		 0, 0,    NULL,      0, NULL }
};

#include <sys/stat.h>

#include "rump_dev_private.h"
#include "rump_vfs_private.h"

#define FLAWLESSCALL(call)						\
do {									\
	int att_error;							\
	if ((att_error = call) != 0)					\
		panic("\"%s\" failed", #call);				\
} while (/*CONSTCOND*/0)

void
rump_wscons_configuration(void)
{
	extern struct cfattach wskbd_ca, wsmouse_ca;
	extern struct cdevsw wskbd_cdevsw, wsmouse_cdevsw;
	devmajor_t bmaj, cmaj;

	FLAWLESSCALL(config_cfdata_attach(cfdata_wscons, 0));

	FLAWLESSCALL(config_cfdriver_attach(&wskbd_cd));
	FLAWLESSCALL(config_cfattach_attach("wskbd", &wskbd_ca));

	FLAWLESSCALL(config_cfdriver_attach(&wsmouse_cd));
	FLAWLESSCALL(config_cfattach_attach("wsmouse", &wsmouse_ca));

	bmaj = cmaj = -1;
	FLAWLESSCALL(devsw_attach("wskbd", NULL, &bmaj, &wskbd_cdevsw, &cmaj));
	FLAWLESSCALL(rump_vfs_makeonedevnode(S_IFCHR, "/dev/wskbd", cmaj, 0));

	bmaj = cmaj = -1;
	FLAWLESSCALL(devsw_attach("wsmouse", NULL, &bmaj,
	    &wsmouse_cdevsw, &cmaj));
	FLAWLESSCALL(rump_vfs_makeonedevnode(S_IFCHR, "/dev/wsmouse", cmaj, 0));
}
