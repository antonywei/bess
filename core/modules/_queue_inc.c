#include "../module.h"
#include "../port.h"

class QueueInc : public Module {
 public:
  virtual struct snobj *Init(struct snobj *arg);
  virtual void Deinit();

  virtual struct task_result RunTask(void *arg);

  virtual struct snobj *GetDesc();

  static const gate_idx_t kNumIGates = 0;
  static const gate_idx_t kNumOGates = 1;

  static const std::vector<struct Command> cmds;

 private:
  struct snobj *CommandSetBurst(struct snobj *arg);

  struct port *port = {0};
  pkt_io_func_t recv_pkts = {0};
  queue_t qid = {0};
  int prefetch = {0};
  int burst = {0};
};

const std::vector<struct Command> QueueInc::cmds = {
    {"set_burst", static_cast<CmdFunc>(&QueueInc::CommandSetBurst), 1},
};

struct snobj *QueueInc::Init(struct snobj *arg) {
  struct snobj *t;
  struct snobj *err;

  const char *port_name;
  task_id_t tid;

  int ret;

  this->burst = MAX_PKT_BURST;

  if (!arg || snobj_type(arg) != TYPE_MAP)
    return snobj_err(EINVAL, "Argument must be a map");

  t = snobj_eval(arg, "port");
  if (!t || !(port_name = snobj_str_get(t)))
    return snobj_err(EINVAL, "Field 'port' must be specified");

  t = snobj_eval(arg, "qid");
  if (!t || snobj_type(t) != TYPE_INT)
    return snobj_err(EINVAL, "Field 'qid' must be specified");
  this->qid = snobj_uint_get(t);

  this->port = find_port(port_name);
  if (!this->port) return snobj_err(ENODEV, "Port %s not found", port_name);

  if ((t = snobj_eval(arg, "burst")) != NULL) {
    err = this->CommandSetBurst(t);
    if (err) return err;
  }

  if (snobj_eval_int(arg, "prefetch")) this->prefetch = 1;

  tid = register_task(this, (void *)(uintptr_t) this->qid);
  if (tid == INVALID_TASK_ID) return snobj_err(ENOMEM, "Task creation failed");

  ret = acquire_queues(this->port, reinterpret_cast<const module *>(this),
                       PACKET_DIR_INC, &this->qid, 1);
  if (ret < 0) return snobj_errno(-ret);

  this->recv_pkts = this->port->driver->recv_pkts;

  return NULL;
}

void QueueInc::Deinit() {
  release_queues(this->port, reinterpret_cast<const module *>(this),
                 PACKET_DIR_INC, &this->qid, 1);
}

struct snobj *QueueInc::GetDesc() {
  return snobj_str_fmt("%s:%hhu/%s", this->port->name, this->qid,
                       this->port->driver->name);
}

struct task_result QueueInc::RunTask(void *arg) {
  struct port *p = this->port;

  const queue_t qid = (queue_t)(uintptr_t)arg;

  struct pkt_batch batch;
  struct task_result ret;

  uint64_t received_bytes = 0;

  const int burst = ACCESS_ONCE(this->burst);
  const int pkt_overhead = 24;

  uint64_t cnt;

  cnt = batch.cnt = this->recv_pkts(p, qid, batch.pkts, burst);

  if (cnt == 0) {
    ret.packets = 0;
    ret.bits = 0;
    return ret;
  }

  /* NOTE: we cannot skip this step since it might be used by scheduler */
  if (this->prefetch) {
    for (uint64_t i = 0; i < cnt; i++) {
      received_bytes += snb_total_len(batch.pkts[i]);
      rte_prefetch0(snb_head_data(batch.pkts[i]));
    }
  } else {
    for (uint64_t i = 0; i < cnt; i++)
      received_bytes += snb_total_len(batch.pkts[i]);
  }

  ret = (struct task_result){
      .packets = cnt, .bits = (received_bytes + cnt * pkt_overhead) * 8,
  };

  if (!(p->driver->flags & DRIVER_FLAG_SELF_INC_STATS)) {
    p->queue_stats[PACKET_DIR_INC][qid].packets += cnt;
    p->queue_stats[PACKET_DIR_INC][qid].bytes += received_bytes;
  }

  run_next_module(this, &batch);

  return ret;
}

struct snobj *QueueInc::CommandSetBurst(struct snobj *arg) {
  uint64_t val;

  if (snobj_type(arg) != TYPE_INT)
    return snobj_err(EINVAL, "burst must be an integer");

  val = snobj_uint_get(arg);

  if (val == 0 || val > MAX_PKT_BURST)
    return snobj_err(EINVAL, "burst size must be [1,%d]", MAX_PKT_BURST);

  this->burst = val;

  return NULL;
}

ModuleClassRegister<QueueInc> queue_inc(
    "QueueInc", "queue_inc",
    "receives packets from a port via a specific queue");
