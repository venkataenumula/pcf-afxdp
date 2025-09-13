// xdp/user/loader.c (excerpt)
int main(int argc, char **argv) {
  const char *ifname = getenv("NIC");
  int ifindex = if_nametoindex(ifname);

  // 1) mount bpffs if needed, 2) load BPF obj, 3) create+pin maps under /sys/fs/bpf/pcf/
  ensure_bpffs_mounted();
  load_bpf_obj("pcf_xdp_kern.o");
  create_and_pin_maps("/sys/fs/bpf/pcf");

  // 4) attach XDP (drv mode preferred)
  bpf_set_link_xdp_fd(ifindex, prog_fd, XDP_FLAGS_DRV_MODE);

  // 5) populate xskmap slots per RX queue (the DPDK app opens XSKs later)
  init_xskmap_for_queues(ifindex, num_queues);

  // stay resident if you want to watch link changes or just exit after attach
  pause();
}

