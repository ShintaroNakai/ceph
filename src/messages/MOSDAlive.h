// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*- 
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2004-2006 Sage Weil <sage@newdream.net>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software 
 * Foundation.  See file COPYING.
 * 
 */



#ifndef CEPH_MOSDALIVE_H
#define CEPH_MOSDALIVE_H

#include "messages/PaxosServiceMessage.h"

class MOSDAlive : public PaxosServiceMessage {
 public:
  epoch_t want;

  MOSDAlive(epoch_t h, epoch_t w) : PaxosServiceMessage(MSG_OSD_ALIVE, h), want(w) { }
  MOSDAlive() : PaxosServiceMessage(MSG_OSD_ALIVE, 0) {}
private:
  ~MOSDAlive() {}

public:
  void encode_payload(CephContext *cct) {
    paxos_encode();
    ::encode(want, payload);
  }
  void decode_payload(CephContext *cct) {
    bufferlist::iterator p = payload.begin();
    paxos_decode(p);
    ::decode(want, p);
  }

  const char *get_type_name() { return "osd_alive"; }
  void print(ostream &out) {
    out << "osd_alive(want up_thru " << want << " have " << version << ")";
  }
  
};

#endif
