/* $NetBSD: ibusvar.h,v 1.7 1999/11/17 00:10:00 nisimura Exp $ */

#ifndef _IBUSVAR_H_
#define _IBUSVAR_H_ 1

#include <machine/bus.h>

struct ibus_attach_args;

struct ibus_softc {
	struct device	sc_dev;

	void	(*sc_intr_establish) __P((struct device *, void *,
					int, int (*)(void *), void *));
	void	(*sc_intr_disestablish) __P((struct device *, void *));
};

/*
 * Arguments used to attach an ibus "device" to its parent
 */
struct ibus_dev_attach_args {
	const char *ida_busname;		/* XXX should be common */
	bus_space_tag_t	ida_memt;

	int	ida_ndevs;
	struct ibus_attach_args	*ida_devs;
	void	(*ida_establish) __P((struct device *, void *,
					int, int (*)(void *), void *));
	void	(*ida_disestablish) __P((struct device *, void *));
};

/*
 * Arguments used to attach devices to an ibus
 */
struct ibus_attach_args {
	char	*ia_name;		/* Device name. */
	int	ia_cookie;		/* Device slot (table entry). */
	u_int32_t ia_addr;		/* Device address. */
};

void ibusattach __P((struct device *, struct device *, void *));
int  ibusprint __P((void *, const char *));
void ibus_intr_establish __P((struct device *, void * cookie, int level,
			int (*handler)(void *), void *arg));
void ibus_intr_disestablish __P((struct device *, void *));

int  badaddr __P((void *, u_int));

#endif /* _IBUSVAR_H_ */
