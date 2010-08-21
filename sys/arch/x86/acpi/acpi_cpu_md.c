/* $NetBSD: acpi_cpu_md.c,v 1.22 2010/08/21 03:55:24 jruoho Exp $ */

/*-
 * Copyright (c) 2010 Jukka Ruohonen <jruohonen@iki.fi>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: acpi_cpu_md.c,v 1.22 2010/08/21 03:55:24 jruoho Exp $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kcore.h>
#include <sys/sysctl.h>
#include <sys/xcall.h>

#include <x86/cpu.h>
#include <x86/cpufunc.h>
#include <x86/cputypes.h>
#include <x86/cpuvar.h>
#include <x86/cpu_msr.h>
#include <x86/machdep.h>

#include <dev/acpi/acpica.h>
#include <dev/acpi/acpi_cpu.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#define CPUID_INTEL_TSC		__BIT(8)

#define MSR_0FH_CONTROL		0xc0010041 /* Family 0Fh (and K7).  */
#define MSR_0FH_STATUS		0xc0010042

#define MSR_10H_LIMIT		0xc0010061 /* Families 10h and 11h. */
#define MSR_10H_CONTROL		0xc0010062
#define MSR_10H_STATUS		0xc0010063
#define MSR_10H_CONFIG		0xc0010064

static char	  native_idle_text[16];
void		(*native_idle)(void) = NULL;

static int	 acpicpu_md_quirks_piix4(struct pci_attach_args *);
static void	 acpicpu_md_pstate_status(void *, void *);
static void	 acpicpu_md_tstate_status(void *, void *);
static int	 acpicpu_md_pstate_sysctl_init(void);
static int	 acpicpu_md_pstate_sysctl_get(SYSCTLFN_PROTO);
static int	 acpicpu_md_pstate_sysctl_set(SYSCTLFN_PROTO);
static int	 acpicpu_md_pstate_sysctl_all(SYSCTLFN_PROTO);

extern uint32_t cpus_running;
extern struct acpicpu_softc **acpicpu_sc;
static struct sysctllog *acpicpu_log = NULL;

uint32_t
acpicpu_md_cap(void)
{
	struct cpu_info *ci = curcpu();
	uint32_t val = 0;

	if (cpu_vendor != CPUVENDOR_IDT &&
	    cpu_vendor != CPUVENDOR_INTEL)
		return val;

	/*
	 * Basic SMP C-states (required for _CST).
	 */
	val |= ACPICPU_PDC_C_C1PT | ACPICPU_PDC_C_C2C3;

        /*
	 * If MONITOR/MWAIT is available, announce
	 * support for native instructions in all C-states.
	 */
        if ((ci->ci_feat_val[1] & CPUID2_MONITOR) != 0)
		val |= ACPICPU_PDC_C_C1_FFH | ACPICPU_PDC_C_C2C3_FFH;

	/*
	 * Set native P- and T-states, if available.
	 */
        if ((ci->ci_feat_val[1] & CPUID2_EST) != 0)
		val |= ACPICPU_PDC_P_FFH;

	if ((ci->ci_feat_val[0] & CPUID_ACPI) != 0)
		val |= ACPICPU_PDC_T_FFH;

	return val;
}

uint32_t
acpicpu_md_quirks(void)
{
	struct cpu_info *ci = curcpu();
	struct pci_attach_args pa;
	uint32_t family, val = 0;
	uint32_t regs[4];

	if (acpicpu_md_cpus_running() == 1)
		val |= ACPICPU_FLAG_C_BM;

	if ((ci->ci_feat_val[1] & CPUID2_MONITOR) != 0)
		val |= ACPICPU_FLAG_C_FFH;

	val |= ACPICPU_FLAG_C_TSC;

	switch (cpu_vendor) {

	case CPUVENDOR_IDT:

		if ((ci->ci_feat_val[1] & CPUID2_EST) != 0)
			val |= ACPICPU_FLAG_P_FFH;

		if ((ci->ci_feat_val[0] & CPUID_ACPI) != 0)
			val |= ACPICPU_FLAG_T_FFH;

		break;

	case CPUVENDOR_INTEL:

		val |= ACPICPU_FLAG_C_BM | ACPICPU_FLAG_C_ARB;

		if ((ci->ci_feat_val[1] & CPUID2_EST) != 0)
			val |= ACPICPU_FLAG_P_FFH;

		if ((ci->ci_feat_val[0] & CPUID_ACPI) != 0)
			val |= ACPICPU_FLAG_T_FFH;

		/*
		 * Detect whether TSC is invariant. If it is not,
		 * we keep the flag to note that TSC will not run
		 * at constant rate. Depending on the CPU, this may
		 * affect P- and T-state changes, but especially
		 * relevant are C-states; with variant TSC, states
		 * larger than C1 will completely stop the timer.
		 */
		x86_cpuid(0x80000000, regs);

		if (regs[0] >= 0x80000007) {

			x86_cpuid(0x80000007, regs);

			if ((regs[3] & CPUID_INTEL_TSC) != 0)
				val &= ~ACPICPU_FLAG_C_TSC;
		}

		break;

	case CPUVENDOR_AMD:

		family = CPUID2FAMILY(ci->ci_signature);

		if (family == 0xf)
			family += CPUID2EXTFAMILY(ci->ci_signature);

		switch (family) {

		case 0x0f:
		case 0x10:
		case 0x11:

			x86_cpuid(0x80000007, regs);

			if ((regs[3] & CPUID_APM_TSC) != 0)
				val &= ~ACPICPU_FLAG_C_TSC;

			if ((regs[3] & CPUID_APM_HWP) != 0)
				val |= ACPICPU_FLAG_P_FFH;

			if ((regs[3] & CPUID_APM_CPB) != 0)
				val |= ACPICPU_FLAG_P_TURBO;
		}

		break;
	}

	/*
	 * There are several erratums for PIIX4.
	 */
	if (pci_find_device(&pa, acpicpu_md_quirks_piix4) != 0)
		val |= ACPICPU_FLAG_PIIX4;

	return val;
}

static int
acpicpu_md_quirks_piix4(struct pci_attach_args *pa)
{

	/*
	 * XXX: The pci_find_device(9) function only
	 *	deals with attached devices. Change this
	 *	to use something like pci_device_foreach().
	 */
	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_INTEL)
		return 0;

	if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_82371AB_ISA ||
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_82440MX_PMC)
		return 1;

	return 0;
}

uint32_t
acpicpu_md_cpus_running(void)
{

	return popcount32(cpus_running);
}

int
acpicpu_md_idle_start(void)
{
	const size_t size = sizeof(native_idle_text);

	x86_disable_intr();
	x86_cpu_idle_get(&native_idle, native_idle_text, size);
	x86_cpu_idle_set(acpicpu_cstate_idle, "acpi");
	x86_enable_intr();

	return 0;
}

int
acpicpu_md_idle_stop(void)
{
	uint64_t xc;

	x86_disable_intr();
	x86_cpu_idle_set(native_idle, native_idle_text);
	x86_enable_intr();

	/*
	 * Run a cross-call to ensure that all CPUs are
	 * out from the ACPI idle-loop before detachment.
	 */
	xc = xc_broadcast(0, (xcfunc_t)nullop, NULL, NULL);
	xc_wait(xc);

	return 0;
}

/*
 * The MD idle loop. Called with interrupts disabled.
 */
void
acpicpu_md_idle_enter(int method, int state)
{
	struct cpu_info *ci = curcpu();

	switch (method) {

	case ACPICPU_C_STATE_FFH:

		x86_enable_intr();
		x86_monitor(&ci->ci_want_resched, 0, 0);

		if (__predict_false(ci->ci_want_resched) != 0)
			return;

		x86_mwait((state - 1) << 4, 0);
		break;

	case ACPICPU_C_STATE_HALT:

		if (__predict_false(ci->ci_want_resched) != 0) {
			x86_enable_intr();
			return;
		}

		x86_stihlt();
		break;
	}
}

int
acpicpu_md_pstate_start(void)
{
	const uint64_t est = __BIT(16);
	uint64_t val;

	switch (cpu_vendor) {

	case CPUVENDOR_IDT:
	case CPUVENDOR_INTEL:

		val = rdmsr(MSR_MISC_ENABLE);

		if ((val & est) == 0) {

			val |= est;

			wrmsr(MSR_MISC_ENABLE, val);
			val = rdmsr(MSR_MISC_ENABLE);

			if ((val & est) == 0)
				return ENOTTY;
		}
	}

	return acpicpu_md_pstate_sysctl_init();
}

int
acpicpu_md_pstate_stop(void)
{

	if (acpicpu_log != NULL)
		sysctl_teardown(&acpicpu_log);

	return 0;
}

int
acpicpu_md_pstate_pss(struct acpicpu_softc *sc)
{
	struct acpicpu_pstate *ps, msr;
	struct cpu_info *ci = curcpu();
	uint32_t family, i = 0;

	(void)memset(&msr, 0, sizeof(struct acpicpu_pstate));

	switch (cpu_vendor) {

	case CPUVENDOR_IDT:
	case CPUVENDOR_INTEL:
		msr.ps_control_addr = MSR_PERF_CTL;
		msr.ps_control_mask = __BITS(0, 15);

		msr.ps_status_addr  = MSR_PERF_STATUS;
		msr.ps_status_mask  = __BITS(0, 15);
		break;

	case CPUVENDOR_AMD:

		family = CPUID2FAMILY(ci->ci_signature);

		if (family == 0xf)
			family += CPUID2EXTFAMILY(ci->ci_signature);

		switch (family) {

		case 0x10:
		case 0x11:
			msr.ps_control_addr = MSR_10H_CONTROL;
			msr.ps_control_mask = __BITS(0, 2);

			msr.ps_status_addr  = MSR_10H_STATUS;
			msr.ps_status_mask  = __BITS(0, 2);
			break;

		default:

			if ((sc->sc_flags & ACPICPU_FLAG_P_XPSS) == 0)
				return EOPNOTSUPP;
		}

		break;

	default:
		return ENODEV;
	}

	while (i < sc->sc_pstate_count) {

		ps = &sc->sc_pstate[i];

		if (ps->ps_status_addr == 0)
			ps->ps_status_addr = msr.ps_status_addr;

		if (ps->ps_status_mask == 0)
			ps->ps_status_mask = msr.ps_status_mask;

		if (ps->ps_control_addr == 0)
			ps->ps_control_addr = msr.ps_control_addr;

		if (ps->ps_control_mask == 0)
			ps->ps_control_mask = msr.ps_control_mask;

		i++;
	}

	return 0;
}

int
acpicpu_md_pstate_get(struct acpicpu_softc *sc, uint32_t *freq)
{
	struct acpicpu_pstate *ps = NULL;
	uint64_t val;
	uint32_t i;

	for (i = 0; i < sc->sc_pstate_count; i++) {

		ps = &sc->sc_pstate[i];

		if (ps->ps_freq != 0)
			break;
	}

	if (__predict_false(ps == NULL))
		return ENODEV;

	if (ps->ps_status_addr == 0)
		return EINVAL;

	val = rdmsr(ps->ps_status_addr);

	if (ps->ps_status_mask != 0)
		val = val & ps->ps_status_mask;

	for (i = 0; i < sc->sc_pstate_count; i++) {

		ps = &sc->sc_pstate[i];

		if (ps->ps_freq == 0)
			continue;

		if (val == ps->ps_status) {
			*freq = ps->ps_freq;
			return 0;
		}
	}

	return EIO;
}

int
acpicpu_md_pstate_set(struct acpicpu_pstate *ps)
{
	struct msr_rw_info msr;
	uint64_t xc;
	int rv = 0;

	msr.msr_read  = false;
	msr.msr_type  = ps->ps_control_addr;
	msr.msr_value = ps->ps_control;

	if (ps->ps_control_mask != 0) {
		msr.msr_mask = ps->ps_control_mask;
		msr.msr_read = true;
	}

	xc = xc_broadcast(0, (xcfunc_t)x86_msr_xcall, &msr, NULL);
	xc_wait(xc);

	if (ps->ps_status_addr == 0)
		return 0;

	xc = xc_broadcast(0, (xcfunc_t)acpicpu_md_pstate_status, ps, &rv);
	xc_wait(xc);

	return rv;
}

static void
acpicpu_md_pstate_status(void *arg1, void *arg2)
{
	struct acpicpu_pstate *ps = arg1;
	uint64_t val;
	int i;

	for (i = val = 0; i < ACPICPU_P_STATE_RETRY; i++) {

		val = rdmsr(ps->ps_status_addr);

		if (ps->ps_status_mask != 0)
			val = val & ps->ps_status_mask;

		if (val == ps->ps_status)
			return;

		DELAY(ps->ps_latency);
	}

	*(uintptr_t *)arg2 = EAGAIN;
}

int
acpicpu_md_tstate_get(struct acpicpu_softc *sc, uint32_t *percent)
{
	struct acpicpu_tstate *ts;
	uint64_t val;
	uint32_t i;

	val = rdmsr(MSR_THERM_CONTROL);

	for (i = 0; i < sc->sc_tstate_count; i++) {

		ts = &sc->sc_tstate[i];

		if (ts->ts_percent == 0)
			continue;

		if (val == ts->ts_control || val == ts->ts_status) {
			*percent = ts->ts_percent;
			return 0;
		}
	}

	return EIO;
}

int
acpicpu_md_tstate_set(struct acpicpu_tstate *ts)
{
	struct msr_rw_info msr;
	uint64_t xc;
	int rv = 0;

	msr.msr_read  = true;
	msr.msr_type  = MSR_THERM_CONTROL;
	msr.msr_value = ts->ts_control;
	msr.msr_mask = __BITS(1, 4);

	xc = xc_broadcast(0, (xcfunc_t)x86_msr_xcall, &msr, NULL);
	xc_wait(xc);

	if (ts->ts_status == 0)
		return 0;

	xc = xc_broadcast(0, (xcfunc_t)acpicpu_md_tstate_status, ts, &rv);
	xc_wait(xc);

	return rv;
}

static void
acpicpu_md_tstate_status(void *arg1, void *arg2)
{
	struct acpicpu_tstate *ts = arg1;
	uint64_t val;
	int i;

	for (i = val = 0; i < ACPICPU_T_STATE_RETRY; i++) {

		val = rdmsr(MSR_THERM_CONTROL);

		if (val == ts->ts_status)
			return;

		DELAY(ts->ts_latency);
	}

	*(uintptr_t *)arg2 = EAGAIN;
}

/*
 * A kludge for backwards compatibility.
 */
static int
acpicpu_md_pstate_sysctl_init(void)
{
	const struct sysctlnode	*fnode, *mnode, *rnode;
	const char *str;
	int rv;

	switch (cpu_vendor) {

	case CPUVENDOR_IDT:
	case CPUVENDOR_INTEL:
		str = "est";
		break;

	case CPUVENDOR_AMD:
		str = "powernow";
		break;

	default:
		return ENODEV;
	}


	rv = sysctl_createv(&acpicpu_log, 0, NULL, &rnode,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, "machdep", NULL,
	    NULL, 0, NULL, 0, CTL_MACHDEP, CTL_EOL);

	if (rv != 0)
		goto fail;

	rv = sysctl_createv(&acpicpu_log, 0, &rnode, &mnode,
	    0, CTLTYPE_NODE, str, NULL,
	    NULL, 0, NULL, 0, CTL_CREATE, CTL_EOL);

	if (rv != 0)
		goto fail;

	rv = sysctl_createv(&acpicpu_log, 0, &mnode, &fnode,
	    0, CTLTYPE_NODE, "frequency", NULL,
	    NULL, 0, NULL, 0, CTL_CREATE, CTL_EOL);

	if (rv != 0)
		goto fail;

	rv = sysctl_createv(&acpicpu_log, 0, &fnode, &rnode,
	    CTLFLAG_READWRITE, CTLTYPE_INT, "target", NULL,
	    acpicpu_md_pstate_sysctl_set, 0, NULL, 0, CTL_CREATE, CTL_EOL);

	if (rv != 0)
		goto fail;

	rv = sysctl_createv(&acpicpu_log, 0, &fnode, &rnode,
	    CTLFLAG_READONLY, CTLTYPE_INT, "current", NULL,
	    acpicpu_md_pstate_sysctl_get, 0, NULL, 0, CTL_CREATE, CTL_EOL);

	if (rv != 0)
		goto fail;

	rv = sysctl_createv(&acpicpu_log, 0, &fnode, &rnode,
	    CTLFLAG_READONLY, CTLTYPE_STRING, "available", NULL,
	    acpicpu_md_pstate_sysctl_all, 0, NULL, 0, CTL_CREATE, CTL_EOL);

	if (rv != 0)
		goto fail;

	return 0;

fail:
	if (acpicpu_log != NULL) {
		sysctl_teardown(&acpicpu_log);
		acpicpu_log = NULL;
	}

	return rv;
}

static int
acpicpu_md_pstate_sysctl_get(SYSCTLFN_ARGS)
{
	struct cpu_info *ci = curcpu();
	struct acpicpu_softc *sc;
	struct sysctlnode node;
	uint32_t freq;
	int err;

	sc = acpicpu_sc[ci->ci_acpiid];

	if (sc == NULL)
		return ENXIO;

	err = acpicpu_pstate_get(sc, &freq);

	if (err != 0)
		return err;

	node = *rnode;
	node.sysctl_data = &freq;

	err = sysctl_lookup(SYSCTLFN_CALL(&node));

	if (err != 0 || newp == NULL)
		return err;

	return 0;
}

static int
acpicpu_md_pstate_sysctl_set(SYSCTLFN_ARGS)
{
	struct cpu_info *ci = curcpu();
	struct acpicpu_softc *sc;
	struct sysctlnode node;
	uint32_t freq;
	int err;

	sc = acpicpu_sc[ci->ci_acpiid];

	if (sc == NULL)
		return ENXIO;

	err = acpicpu_pstate_get(sc, &freq);

	if (err != 0)
		return err;

	node = *rnode;
	node.sysctl_data = &freq;

	err = sysctl_lookup(SYSCTLFN_CALL(&node));

	if (err != 0 || newp == NULL)
		return err;

	err = acpicpu_pstate_set(sc, freq);

	if (err != 0)
		return err;

	return 0;
}

static int
acpicpu_md_pstate_sysctl_all(SYSCTLFN_ARGS)
{
	struct cpu_info *ci = curcpu();
	struct acpicpu_softc *sc;
	struct sysctlnode node;
	char buf[1024];
	size_t len;
	uint32_t i;
	int err;

	sc = acpicpu_sc[ci->ci_acpiid];

	if (sc == NULL)
		return ENXIO;

	(void)memset(&buf, 0, sizeof(buf));

	mutex_enter(&sc->sc_mtx);

	for (len = 0, i = sc->sc_pstate_max; i < sc->sc_pstate_count; i++) {

		if (sc->sc_pstate[i].ps_freq == 0)
			continue;

		len += snprintf(buf + len, sizeof(buf) - len, "%u%s",
		    sc->sc_pstate[i].ps_freq,
		    i < (sc->sc_pstate_count - 1) ? " " : "");
	}

	mutex_exit(&sc->sc_mtx);

	node = *rnode;
	node.sysctl_data = buf;

	err = sysctl_lookup(SYSCTLFN_CALL(&node));

	if (err != 0 || newp == NULL)
		return err;

	return 0;
}

