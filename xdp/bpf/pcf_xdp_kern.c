// xdp/bpf/pcf_xdp_kern.c (excerpt)
#include "pcf_xdp_maps.h"
SEC("xdp") int pcf_xdp(struct xdp_md *ctx) {
  void *data = (void *)(long)ctx->data;
  void *data_end = (void *)(long)ctx->data_end;

  struct pkt_md md = {};
  if (parse_5tuple(data, data_end, &md) < 0) return XDP_PASS;

  struct action *act = bpf_map_lookup_elem(&pcf_flow_map, &md.key);
  if (!act) return XDP_PASS;

  if (act->type == ACT_DROP) return XDP_DROP;
  if (act->type == ACT_PASS) return XDP_PASS;

  // redirect to AF_XDP socket (queue chosen in control plane)
  __u32 qid = act->queue_id;
  int rc = bpf_redirect_map(&pcf_xskmap, qid, 0);
  return rc;
}
char _license[] SEC("license") = "GPL";

