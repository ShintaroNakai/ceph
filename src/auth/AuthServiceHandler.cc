// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*- 
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2004-2009 Sage Weil <sage@newdream.net>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software 
 * Foundation.  See file COPYING.
 * 
 */

#include "AuthServiceHandler.h"
#include "cephx/CephxServiceHandler.h"
#include "none/AuthNoneServiceHandler.h"
#include "AuthSupported.h"
#include "common/config.h"

#define DOUT_SUBSYS auth


AuthServiceHandler *get_auth_service_handler(CephContext *cct, KeyServer *ks,
					     set<__u32>& supported)
{
  if (is_supported_auth(CEPH_AUTH_CEPHX, cct) && supported.count(CEPH_AUTH_CEPHX))
    return new CephxServiceHandler(cct, ks);
  if (is_supported_auth(CEPH_AUTH_NONE, cct) && supported.count(CEPH_AUTH_NONE))
    return new AuthNoneServiceHandler(cct);
  return NULL;
}


