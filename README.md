# pcf-afxdp
AF_XDP + DPDK fast path with a tiny XDP steering layer

# What it is? 
  Ingress steering (XDP): a minimal eBPF/XDP program on host NIC(s) reads a pinned BPF map of flow rules and decides: PASS (stay in kernel), REDIRECT (to AF_XDP ring for user-space fast path), or DROP.
  
  User-space data plane (DPDK AF_XDP): a containerized DPDK service binds to AF_XDP sockets (one per RX queue), processes packets (TTL rewrite, ACL, NAT, mirror, counters), and transmits either:
  
  back to the kernel (via AF_XDP TX → devmap redirect), or
  
  directly out the NIC queue (still via AF_XDP path).
  
  Control plane (genetlink or userspace API): a small controller pushes 5-tuple rules and actions into pinned maps; exposes REST/gRPC for ops, and syncs with your existing PCF logic if you keep it.

# Why this is nice
  
  No SR-IOV needed.
  
  Kernel networking continues to work for non-selected traffic.
  
  Clean rollback: if DPDK dies or XDP is unloaded, traffic can default to PASS.

# Data path (packet journey)

  NIC RX → XDP (ns/µs): hash on 5-tuple → lookup pcf_flow_map.
  
  Action = REDIRECT: XDP redirects to an xskmap entry (queue-pinned AF_XDP socket).
  
  DPDK loop: batch pull from AF_XDP RX ring → run PCF pipeline (ACL → NAT → TTL → log/metrics → verdict).
  
  Egress:
  
    Return to kernel: enqueue to AF_XDP TX; XDP TX helper forwards back to NIC/kernel path.
  
    Bypass: enqueue to a TX ring that directly emits to wire (still via XDP/AF_XDP path).

# Control plane interface
Rule model (pseudocode):
      {
        "key": { "src_ip": "...", "dst_ip": "...", "src_port": 1234, "dst_port": 80, "proto": 6 },
        "action": { "type": "REDIRECT" | "PASS" | "DROP",
                    "ttl_delta": -1,
                    "nat": { "snat_ip": "...", "snat_port": 0, "dnat_ip": null, "dnat_port": 0 },
                    "mark": 273,
                    "queue_id": 0 }
      }

# APIs (REST or gRPC):
  
      POST /flows → upsert rule (also writes to pinned BPF map)
      
      DELETE /flows/{id} → remove rule
      
      GET /stats → counters (XDP + DPDK)


# Maps (pinned at /sys/fs/bpf/pcf/)
  
      pcf_flow_map (LPM/_hash): 5-tuple → compact action struct
      
      pcf_xskmap (xskmap): queue_id → AF_XDP socket fd
      
      pcf_stats (per-CPU array/hash): hits/drops per action/queue
    

# POC plan
  
    Prep a node with NIC ens5f0, reserve 1–2GiB hugepages.
    
    Apply XDP DaemonSet → confirm ip link show reports prog/xdp attached.
    
    Apply DPDK DaemonSet → logs show queues opened & RX/TX running.
    
    Push a flow rule via control plane
    
    Generate traffic (hitting the 5-tuple) and verify:
    
        XDP stats: bpftool prog tracelog or read pcf_stats map via helper.
        
        DPDK counters increase; TTL decremented on egress.
    
    Kill DPDK pod → traffic should fallback to PASS (no blackhole).


# Security & compliance notes
  
  Works with Secure Boot (no custom kernel modules needed); XDP/eBPF are signed via kernel verifier.
  
  Least privilege: you can drop --privileged by adding CAP_BPF, CAP_NET_ADMIN, SYS_RESOURCE and mounting bpffs + hugepages explicitly.
  
  FIPS: control plane crypto (if any) should use FIPS-approved libs; data plane doesn’t require crypto.


# Build & run (POC path)
Node prep (RHEL 8/9 example) :
    
    sudo ./scripts/prepare-node.sh \
      --nic ens5f0 \
      --huge-mb 1024 \
      --queues 2
    # does: hugepages setup, mount /sys/fs/bpf, disable GRO/LRO on the target NIC queues, set RPS/XPS

Build locally:

    # XDP
    pushd xdp && make V=1 && popd
    # DPDK app (assumes DPDK dev libs or builds with container)
    pushd dpdk && cmake -B build && cmake --build build -j && popd
    # Control plane
    pushd control && go build -o pcfctl ./... && popd

Docker images:

    make -f build/docker.mk dpdk-image    # builds your-registry/pcf-dpdk:latest
    make -f build/docker.mk ctl-image     # builds your-registry/pcf-ctl:latest




