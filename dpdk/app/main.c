// dpdk/app/main.c (excerpt)
int main(int argc, char **argv) {
  // EAL init with afxdp PMD enabled
  rte_eal_init(argc, argv);

  // create one AF_XDP port per NIC queue you want to consume
  uint16_t port_id = init_afxdp_ports(nic_name, queues);

  // ring buffers, mbuf pool, pipeline init
  init_pipelines();

  while (likely(run)) {
    const uint16_t nb_rx = rte_eth_rx_burst(port_id, qid, pkts, BURST);
    if (!nb_rx) continue;

    // ACL -> NAT -> TTL -> mirror/log
    process_acl(pkts, nb_rx);
    process_nat(pkts, nb_rx);
    process_ttl(pkts, nb_rx);

    // TX back (kernel or wire) — typically same port/qid
    rte_eth_tx_burst(port_id, qid, pkts, nb_rx);
    stats.pkts += nb_rx;
  }
}
