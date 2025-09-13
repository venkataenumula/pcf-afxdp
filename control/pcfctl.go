// control/pcfctl.go (excerpt)
type FlowRule struct { Key FiveTuple; Action Action }
func (s *Server) AddFlow(w http.ResponseWriter, r *http.Request) {
  var fr FlowRule
  json.NewDecoder(r.Body).Decode(&fr)
  // write into pinned hash map /sys/fs/bpf/pcf/pcf_flow_map
  if err := bpfmaps.UpsertFlow(fr); err != nil { http.Error(w, err.Error(), 500); return }
  w.WriteHeader(201)
}

