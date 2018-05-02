#ifndef __RING_HELPERS_H
#define __RING_HELPERS_H

static __always_inline void write_evt_hdr(struct filler_data *data)
{
	struct ppm_evt_hdr *evt_hdr = (struct ppm_evt_hdr *)data->buf;

	data->curarg_already_on_frame = false;
	data->curarg = 0;

	evt_hdr->ts = data->tail_ctx.ts;
	evt_hdr->tid = bpf_get_current_pid_tgid() & 0xffffffff;
	evt_hdr->type = data->tail_ctx.evt_type;

	data->curoff = sizeof(struct ppm_evt_hdr)
			+ sizeof(u16) * data->evt->nparams;

	data->len = data->curoff;
}

static __always_inline void fixup_evt_len(char *p, unsigned long len)
{
	struct ppm_evt_hdr *evt_hdr = (struct ppm_evt_hdr *)p;

	evt_hdr->len = len;
}

static __always_inline void fixup_evt_arg_len(char *p, unsigned int argnum,
					      unsigned int arglen)
{
	volatile unsigned int argnumv = argnum;
	*((u16 *)&p[sizeof(struct ppm_evt_hdr)] + (argnumv & (PPM_MAX_EVENT_PARAMS - 1))) = arglen;
}

static __always_inline int push_evt_frame(void *ctx, struct filler_data *data)
{
	if (data->curarg != data->evt->nparams) {
		PRINTK("corrupted filler for event type %d (added %u args, should have added %u)\n",
		       data->tail_ctx.evt_type, data->curarg,
		       data->evt->nparams);
		return PPM_FAILURE_BUG;
	}

	fixup_evt_len(data->buf, data->len);

#ifdef BPF_FORBIDS_ZERO_ACCESS
	int res = bpf_perf_event_output(ctx,
					&perf_map,
					BPF_F_CURRENT_CPU,
					data->buf,
					((data->len - 1) & SCRATCH_SIZE_MAX) + 1);
#else
	int res = bpf_perf_event_output(ctx,
					&perf_map,
					BPF_F_CURRENT_CPU,
					data->buf,
					data->len & SCRATCH_SIZE_MAX);
#endif
	if (res) {
		PRINTK("bpf_perf_event_output failed: %d\n", res);
		return PPM_FAILURE_BUG;
	}

	return PPM_SUCCESS;
}

#endif