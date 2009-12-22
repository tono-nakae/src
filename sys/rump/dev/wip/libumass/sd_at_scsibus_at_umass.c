/*	$NetBSD: sd_at_scsibus_at_umass.c,v 1.6 2009/12/22 13:34:35 pooka Exp $	*/

/*
 * MACHINE GENERATED: DO NOT EDIT
 *
 * ioconf.c, from the following config:
 *
 * === SNIP ===

include "conf/files"
include "dev/usb/files.usb"
include "dev/scsipi/files.scsipi"

device mainbus { }
attach mainbus at root
device rumpusbhc: usbus, usbroothub
attach rumpusbhc at mainbus

mainbus0 at root
rumpusbhc* at mainbus0

# USB bus support
usb*    at rumpusbhc?

# USB Hubs
uhub*   at usb?
uhub*   at uhub? port ?

# USB Mass Storage
umass*  at uhub? port ? configuration ? interface ?
scsibus* at scsi?
sd*     at scsibus? target ? lun ?

 * === UNSNIP ===
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

static const struct cfiattrdata * const usb_attrs[] = { &usbroothubifcf_iattrdata, NULL };
CFDRIVER_DECL(usb, DV_DULL, usb_attrs);

static const struct cfiattrdata * const uhub_attrs[] = { &usbififcf_iattrdata, &usbdevifcf_iattrdata, NULL };
CFDRIVER_DECL(uhub, DV_DULL, uhub_attrs);

static const struct cfiattrdata * const umass_attrs[] = { &scsicf_iattrdata, NULL };
CFDRIVER_DECL(umass, DV_DULL, umass_attrs);

static const struct cfiattrdata * const scsibus_attrs[] = { &scsibuscf_iattrdata, NULL };
CFDRIVER_DECL(scsibus, DV_DULL, scsibus_attrs);

CFDRIVER_DECL(sd, DV_DISK, NULL);

static const struct cfiattrdata * const rumpusbhc_attrs[] = { &usbuscf_iattrdata, NULL };
CFDRIVER_DECL(rumpusbhc, DV_DULL, rumpusbhc_attrs);



extern struct cfattach usb_ca;
extern struct cfattach uroothub_ca;
extern struct cfattach uhub_ca;
extern struct cfattach umass_ca;
extern struct cfattach scsibus_ca;
extern struct cfattach sd_ca;
extern struct cfattach rumpusbhc_ca;

/* locators */
static int loc[15] = {
        -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1,
};

static const struct cfparent pspec1 = {
	"usbus", "rumpusbhc", DVUNIT_ANY
};
static const struct cfparent pspec2 = {
	"usbroothubif", "usb", DVUNIT_ANY
};
static const struct cfparent pspec3 = {
	"usbdevif", "uhub", DVUNIT_ANY
};
static const struct cfparent pspec4 = {
	"usbifif", "uhub", DVUNIT_ANY
};
static const struct cfparent pspec5 = {
	"scsi", NULL, 0
};
static const struct cfparent pspec6 = {
	"scsibus", "scsibus", DVUNIT_ANY
};

#define NORM FSTATE_NOTFOUND
#define STAR FSTATE_STAR

struct cfdata cfdata_umass[] = {
    /* driver           attachment    unit state loc   flags pspec */
/*  0: usb* at rumpusbhc? */
    { "usb",		"usb",		 0, STAR,     loc,      0, &pspec1 },
/*  1: uhub* at usb? */
    { "uhub",		"uroothub",	 0, STAR,     loc,      0, &pspec2 },
/*  2: uhub* at uhub? port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    { "uhub",		"uhub",		 0, STAR,     loc,      0, &pspec3 },
/*  3: umass* at uhub? port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    { "umass",		"umass",	 0, STAR,     loc+6,      0, &pspec4 },
/*  4: scsibus* at scsi? channel -1 */
    { "scsibus",		"scsibus",	 0, STAR,     loc+14,      0, &pspec5 },
/*  5: sd* at scsibus? target -1 lun -1 */
    { "sd",		"sd",		 0, STAR,     loc+12,      0, &pspec6 },
    { NULL,		NULL,		 0, 0,    NULL,      0, NULL }
};

/**** END AUTOGENERATED STUFF ****/


#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/kmem.h>
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
rump_device_configuration(void)
{
	extern struct cfattach usb_ca, uhub_ca, uroothub_ca, umass_ca;
	extern struct cfattach scsibus_ca, sd_ca;
	extern struct bdevsw sd_bdevsw;
	extern struct cdevsw sd_cdevsw;
	devmajor_t bmaj, cmaj;

	FLAWLESSCALL(config_cfdata_attach(cfdata_umass, 0));

	FLAWLESSCALL(config_cfdriver_attach(&usb_cd));
	FLAWLESSCALL(config_cfattach_attach("usb", &usb_ca));

	FLAWLESSCALL(config_cfdriver_attach(&uhub_cd));
	FLAWLESSCALL(config_cfattach_attach("uhub", &uhub_ca));

	FLAWLESSCALL(config_cfdriver_attach(&umass_cd));
	FLAWLESSCALL(config_cfattach_attach("umass", &umass_ca));

	FLAWLESSCALL(config_cfdriver_attach(&scsibus_cd));
	FLAWLESSCALL(config_cfattach_attach("scsibus", &scsibus_ca));

	FLAWLESSCALL(config_cfdriver_attach(&sd_cd));
	FLAWLESSCALL(config_cfattach_attach("sd", &sd_ca));

	FLAWLESSCALL(config_cfattach_attach("uhub", &uroothub_ca));

	bmaj = cmaj = -1;
	FLAWLESSCALL(devsw_attach("sd", &sd_bdevsw, &bmaj, &sd_cdevsw, &cmaj));

	FLAWLESSCALL(rump_vfs_makedevnodes(S_IFBLK, "/dev/sd0", 'a',
	    bmaj, 0, 8));
	FLAWLESSCALL(rump_vfs_makedevnodes(S_IFCHR, "/dev/rsd0", 'a',
	    cmaj, 0, 8));
}
