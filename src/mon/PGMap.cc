
#include "PGMap.h"

#define DOUT_SUBSYS mon
#include "common/debug.h"

void PGMap::apply_incremental(const Incremental& inc)
{
  assert(inc.version == version+1);
  version++;
  bool ratios_changed = false;
  if (inc.full_ratio != 0) {
    full_ratio = inc.full_ratio;
    ratios_changed = true;
  }
  if (inc.nearfull_ratio != 0) {
    nearfull_ratio = inc.nearfull_ratio;
    ratios_changed = true;
  }
  if (ratios_changed)
    redo_full_sets();
  for (map<pg_t,pg_stat_t>::const_iterator p = inc.pg_stat_updates.begin();
       p != inc.pg_stat_updates.end();
       ++p) {
    const pg_t &update_pg(p->first);
    const pg_stat_t &update_stat(p->second);

    hash_map<pg_t,pg_stat_t>::iterator t = pg_stat.find(update_pg);
    if (t == pg_stat.end()) {
      hash_map<pg_t,pg_stat_t>::value_type v(update_pg, update_stat);
      pg_stat.insert(v);
    } else {
      stat_pg_sub(update_pg, t->second);
      t->second = update_stat;
    }
    stat_pg_add(update_pg, update_stat);
  }
  for (map<int,osd_stat_t>::const_iterator p = inc.osd_stat_updates.begin();
       p != inc.osd_stat_updates.end();
       ++p) {
    int osd = p->first;
    const osd_stat_t &new_stats(p->second);
    
    hash_map<int,osd_stat_t>::iterator t = osd_stat.find(osd);
    if (t == osd_stat.end()) {
      hash_map<int,osd_stat_t>::value_type v(osd, new_stats);
      osd_stat.insert(v);
    }
    else {
      stat_osd_sub(t->second);
      t->second = new_stats;
    }
    
    stat_osd_add(new_stats);
    
    //update the full/nearful_osd sets
    int from = p->first;
    float ratio = ((float)p->second.kb_used) / (float) p->second.kb;
    if ( ratio > full_ratio ) {
      full_osds.insert(from);
      //sets don't double-insert so this might be a (very expensive) null-op
    }
    else if ( ratio > nearfull_ratio ) {
      nearfull_osds.insert(from);
      full_osds.erase(from);
    }
    else {//it's not full or near-full
      full_osds.erase(from);
      nearfull_osds.erase(from);
    }
  }
  for (set<pg_t>::const_iterator p = inc.pg_remove.begin();
       p != inc.pg_remove.end();
       p++) {
    const pg_t &removed_pg(*p);
    hash_map<pg_t,pg_stat_t>::iterator s = pg_stat.find(removed_pg);
    if (s != pg_stat.end()) {
      stat_pg_sub(removed_pg, s->second);
      pg_stat.erase(s);
    }
  }
  
  for (set<int>::iterator p = inc.osd_stat_rm.begin();
       p != inc.osd_stat_rm.end();
       p++) {
    hash_map<int,osd_stat_t>::iterator t = osd_stat.find(*p);
    if (t != osd_stat.end()) {
      stat_osd_sub(t->second);
      osd_stat.erase(t);
    }
  }
  
  if (inc.osdmap_epoch)
    last_osdmap_epoch = inc.osdmap_epoch;
  if (inc.pg_scan)
    last_pg_scan = inc.pg_scan;
}

void PGMap::redo_full_sets()
{
  full_osds.clear();
  nearfull_osds.clear();
  for (hash_map<int, osd_stat_t>::iterator i = osd_stat.begin();
       i != osd_stat.end();
       ++i) {
    float ratio = ((float)i->second.kb_used) / ((float)i->second.kb);
    if ( ratio > full_ratio )
      full_osds.insert(i->first);
    else if ( ratio > nearfull_ratio )
      nearfull_osds.insert(i->first);
  }
}

void PGMap::stat_zero()
{
  num_pg = 0;
  num_pg_by_state.clear();
  num_osd = 0;
  pg_pool_sum.clear();
  pg_sum = pool_stat_t();
  osd_sum = osd_stat_t();
}

void PGMap::stat_pg_add(const pg_t &pgid, const pg_stat_t &s)
{
  num_pg++;
  num_pg_by_state[s.state]++;
  pg_pool_sum[pgid.pool()].add(s);
  pg_sum.add(s);
  if (s.state & PG_STATE_CREATING)
    creating_pgs.insert(pgid);
}

void PGMap::stat_pg_sub(const pg_t &pgid, const pg_stat_t &s)
{
  num_pg--;
  if (--num_pg_by_state[s.state] == 0)
    num_pg_by_state.erase(s.state);
  pg_pool_sum[pgid.pool()].sub(s);
  pg_sum.sub(s);
  if (s.state & PG_STATE_CREATING)
    creating_pgs.erase(pgid);
}

void PGMap::stat_osd_add(const osd_stat_t &s)
{
  num_osd++;
  osd_sum.add(s);
}

void PGMap::stat_osd_sub(const osd_stat_t &s)
{
  num_osd--;
  osd_sum.sub(s);
}

epoch_t PGMap::calc_min_last_epoch_clean() const
{
  if (pg_stat.empty())
    return 0;
  hash_map<pg_t,pg_stat_t>::const_iterator p = pg_stat.begin();
  epoch_t min = p->second.last_epoch_clean;
  for (++p; p != pg_stat.end(); ++p) {
    if (p->second.last_epoch_clean < min)
      min = p->second.last_epoch_clean;
  }
  return min;
}

void PGMap::encode(bufferlist &bl)
{
  __u8 v = 3;
  ::encode(v, bl);
  ::encode(version, bl);
  ::encode(pg_stat, bl);
  ::encode(osd_stat, bl);
  ::encode(last_osdmap_epoch, bl);
  ::encode(last_pg_scan, bl);
  ::encode(full_ratio, bl);
  ::encode(nearfull_ratio, bl);
}

void PGMap::decode(bufferlist::iterator &bl)
{
  __u8 v;
  ::decode(v, bl);
  ::decode(version, bl);
  if (v < 3) {
    pg_stat.clear();
    __u32 n;
    ::decode(n, bl);
    while (n--) {
      old_pg_t opgid;
      ::decode(opgid, bl);
      pg_t pgid = opgid;
      ::decode(pg_stat[pgid], bl);
    }
  } else {
    ::decode(pg_stat, bl);
  }
  ::decode(osd_stat, bl);
  ::decode(last_osdmap_epoch, bl);
  ::decode(last_pg_scan, bl);
  if (v >= 2) {
    ::decode(full_ratio, bl);
    ::decode(nearfull_ratio, bl);
  }
  stat_zero();
  for (hash_map<pg_t,pg_stat_t>::iterator p = pg_stat.begin();
       p != pg_stat.end();
       ++p) {
    stat_pg_add(p->first, p->second);
  }
  for (hash_map<int,osd_stat_t>::iterator p = osd_stat.begin();
       p != osd_stat.end();
       ++p)
    stat_osd_add(p->second);
  
  redo_full_sets();
}

void PGMap::dump(Formatter *f) const
{
  dump_basic(f);
  dump_pg_stats(f);
  dump_pool_stats(f);
  dump_osd_stats(f);
}

void PGMap::dump_basic(Formatter *f) const
{
  f->dump_unsigned("version", version);
  f->dump_unsigned("last_osdmap_epoch", last_osdmap_epoch);
  f->dump_unsigned("last_pg_scan", last_pg_scan);
  f->dump_float("full_ratio", full_ratio);
  f->dump_float("near_full_ratio", nearfull_ratio);
  
  f->open_object_section("pg_stats_sum");
  pg_sum.dump(f);
  f->close_section();
  
  f->open_object_section("osd_stats_sum");
  osd_sum.dump(f);
  f->close_section();
}

void PGMap::dump_pg_stats(Formatter *f) const
{
  f->open_array_section("pg_stats");
  for (hash_map<pg_t,pg_stat_t>::const_iterator i = pg_stat.begin();
       i != pg_stat.end();
       ++i) {
    f->open_object_section("pg_stat");
    f->dump_stream("pgid") << i->first;
    i->second.dump(f);
    f->close_section();
  }
  f->close_section();
}

void PGMap::dump_pool_stats(Formatter *f) const
{
  f->open_array_section("pool_stats");
  for (hash_map<int,pool_stat_t>::const_iterator p = pg_pool_sum.begin();
       p != pg_pool_sum.end();
       ++p) {
    f->open_object_section("pool_stat");
    f->dump_int("poolid", p->first);
    p->second.dump(f);
    f->close_section();
  }
  f->close_section();
}

void PGMap::dump_osd_stats(Formatter *f) const
{
  f->open_array_section("osd_stats");
  for (hash_map<int,osd_stat_t>::const_iterator q = osd_stat.begin();
       q != osd_stat.end();
       ++q) {
    f->open_object_section("osd_stat");
    f->dump_int("osd", q->first);
    q->second.dump(f);
    f->close_section();
  }
  f->close_section();
}


void PGMap::dump(ostream& ss) const
{
  ss << "version " << version << std::endl;
  ss << "last_osdmap_epoch " << last_osdmap_epoch << std::endl;
  ss << "last_pg_scan " << last_pg_scan << std::endl;
  ss << "full_ratio " << full_ratio << std::endl;
  ss << "nearfull_ratio " << nearfull_ratio << std::endl;
  ss << "pg_stat\tobjects\tmip\tdegr\tunf\tkb\tbytes\tlog\tdisklog\tstate\tv\treported\tup\tacting\tlast_scrub" << std::endl;
  for (hash_map<pg_t,pg_stat_t>::const_iterator i = pg_stat.begin();
       i != pg_stat.end(); ++i) {
    const pg_stat_t &st(i->second);
    ss << i->first
       << "\t" << st.stats.sum.num_objects
      //<< "\t" << st.num_object_copies
       << "\t" << st.stats.sum.num_objects_missing_on_primary
       << "\t" << st.stats.sum.num_objects_degraded
       << "\t" << st.stats.sum.num_objects_unfound
       << "\t" << st.stats.sum.num_kb
       << "\t" << st.stats.sum.num_bytes
       << "\t" << st.log_size
       << "\t" << st.ondisk_log_size
       << "\t" << pg_state_string(st.state)
       << "\t" << st.version
       << "\t" << st.reported
       << "\t" << st.up
       << "\t" << st.acting
       << "\t" << st.last_scrub << "\t" << st.last_scrub_stamp
       << std::endl;
  }
  for (hash_map<int,pool_stat_t>::const_iterator p = pg_pool_sum.begin();
       p != pg_pool_sum.end();
       p++)
    ss << "pool " << p->first
       << "\t" << p->second.stats.sum.num_objects
      //<< "\t" << p->second.num_object_copies
       << "\t" << p->second.stats.sum.num_objects_missing_on_primary
       << "\t" << p->second.stats.sum.num_objects_degraded
       << "\t" << p->second.stats.sum.num_objects_unfound
       << "\t" << p->second.stats.sum.num_kb
       << "\t" << p->second.stats.sum.num_bytes
       << "\t" << p->second.log_size
       << "\t" << p->second.ondisk_log_size
       << std::endl;
  ss << " sum\t" << pg_sum.stats.sum.num_objects
    //<< "\t" << pg_sum.num_object_copies
     << "\t" << pg_sum.stats.sum.num_objects_missing_on_primary
     << "\t" << pg_sum.stats.sum.num_objects_unfound
     << "\t" << pg_sum.stats.sum.num_objects_degraded
     << "\t" << pg_sum.stats.sum.num_kb
     << "\t" << pg_sum.stats.sum.num_bytes
     << "\t" << pg_sum.log_size
     << "\t" << pg_sum.ondisk_log_size
     << std::endl;
  ss << "osdstat\tkbused\tkbavail\tkb\thb in\thb out" << std::endl;
  for (hash_map<int,osd_stat_t>::const_iterator p = osd_stat.begin();
       p != osd_stat.end();
       p++)
    ss << p->first
       << "\t" << p->second.kb_used
       << "\t" << p->second.kb_avail 
       << "\t" << p->second.kb
       << "\t" << p->second.hb_in
       << "\t" << p->second.hb_out
       << std::endl;
  ss << " sum\t" << osd_sum.kb_used
     << "\t" << osd_sum.kb_avail 
     << "\t" << osd_sum.kb
     << std::endl;
}

void PGMap::state_summary(ostream& ss) const
{
  for (hash_map<int,int>::const_iterator p = num_pg_by_state.begin();
       p != num_pg_by_state.end();
       ++p) {
    if (p != num_pg_by_state.begin())
      ss << ", ";
    ss << p->second << " " << pg_state_string(p->first);
  }
}

void PGMap::recovery_summary(ostream& out) const
{
  bool first = true;
  if (pg_sum.stats.sum.num_objects_degraded) {
    double pc = (double)pg_sum.stats.sum.num_objects_degraded / (double)pg_sum.stats.sum.num_object_copies * (double)100.0;
    char b[20];
    snprintf(b, sizeof(b), "%.3lf", pc);
    out << pg_sum.stats.sum.num_objects_degraded 
	<< "/" << pg_sum.stats.sum.num_object_copies << " degraded (" << b << "%)";
    first = false;
  }
  if (pg_sum.stats.sum.num_objects_unfound) {
    double pc = (double)pg_sum.stats.sum.num_objects_unfound / (double)pg_sum.stats.sum.num_objects * (double)100.0;
    char b[20];
    snprintf(b, sizeof(b), "%.3lf", pc);
    if (!first)
      out << "; ";
    out << pg_sum.stats.sum.num_objects_unfound
	<< "/" << pg_sum.stats.sum.num_objects << " unfound (" << b << "%)";
    first = false;
  }
}

void PGMap::print_summary(ostream& out) const
{
  std::stringstream ss;
  state_summary(ss);
  string states = ss.str();
  out << "v" << version << ": "
      << pg_stat.size() << " pgs: "
      << states << "; "
      << kb_t(pg_sum.stats.sum.num_kb) << " data, " 
      << kb_t(osd_sum.kb_used) << " used, "
      << kb_t(osd_sum.kb_avail) << " / "
      << kb_t(osd_sum.kb) << " avail";
  std::stringstream ssr;
  recovery_summary(ssr);
  if (ssr.str().length())
    out << "; " << ssr.str();
}

