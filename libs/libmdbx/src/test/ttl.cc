﻿/*
 * Copyright 2017-2019 Leonid Yuriev <leo@yuriev.ru>
 * and other libmdbx authors: please see AUTHORS file.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in the file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */

#include "test.h"
#include <cmath>
#include <deque>

static unsigned edge2window(uint64_t edge, unsigned window_max) {
  const double rnd = u64_to_double1(bleach64(edge));
  const unsigned window = window_max - std::lrint(std::pow(window_max, rnd));
  return window;
}

static unsigned edge2count(uint64_t edge, unsigned count_max) {
  const double rnd = u64_to_double1(prng64_map1_white(edge));
  const unsigned count = std::lrint(std::pow(count_max, rnd));
  return count;
}

bool testcase_ttl::run() {
  db_open();

  txn_begin(false);
  MDBX_dbi dbi = db_table_open(true);
  db_table_clear(dbi);
  txn_end(false);

  /* LY: тест "эмуляцией time-to-live":
   *  - организуется "скользящее окно", которое двигается вперед вдоль
   *    числовой оси каждую транзакцию.
   *  - по переднему краю "скользящего окна" записи добавляются в таблицу,
   *    а по заднему удаляются.
   *  - количество добавляемых/удаляемых записей псевдослучайно зависит
   *    от номера транзакции, но с экспоненциальным распределением.
   *  - размер "скользящего окна" также псевдослучайно зависит от номера
   *    транзакции с "отрицательным" экспоненциальным распределением
   *    MAX_WIDTH - exp(rnd(N)), при уменьшении окна сдвигается задний
   *    край и удаляются записи позади него.
   *
   *  Таким образом имитируется поведение таблицы с TTL: записи стохастически
   *  добавляются и удаляются, но изредка происходят массивные удаления.
   */

  /* LY: для параметризации используем подходящие параметры, которые не имеют
   * здесь смысла в первоначальном значении */
  const unsigned window_max =
      (config.params.batch_read > 999) ? config.params.batch_read : 1000;
  const unsigned count_max =
      (config.params.batch_write > 999) ? config.params.batch_write : 1000;
  log_info("ttl: using `batch_read` value %u for window_max", window_max);
  log_info("ttl: using `batch_write` value %u for count_max", count_max);

  uint64_t seed =
      prng64_map2_white(config.params.keygen.seed) + config.actor_id;
  keyvalue_maker.setup(config.params, config.actor_id, 0 /* thread_number */);
  key = keygen::alloc(config.params.keylen_max);
  data = keygen::alloc(config.params.datalen_max);
  const unsigned insert_flags = (config.params.table_flags & MDBX_DUPSORT)
                                    ? MDBX_NODUPDATA
                                    : MDBX_NODUPDATA | MDBX_NOOVERWRITE;

  std::deque<std::pair<uint64_t, unsigned>> fifo;
  uint64_t serial = 0;
  while (should_continue()) {
    if (!txn_guard)
      txn_begin(false);
    const uint64_t salt = prng64_white(seed) /* mdbx_txn_id(txn_guard.get()) */;

    const unsigned window_width = edge2window(salt, window_max);
    const unsigned head_count = edge2count(salt, count_max);
    log_info("ttl: step #%zu (serial %" PRIu64
             ", window %u, count %u) salt %" PRIu64,
             nops_completed, serial, window_width, head_count, salt);

    if (window_width) {
      while (fifo.size() > window_width) {
        uint64_t tail_serial = fifo.back().first;
        const unsigned tail_count = fifo.back().second;
        log_trace("ttl: pop-tail (serial %" PRIu64 ", count %u)", tail_serial,
                  tail_count);
        fifo.pop_back();
        for (unsigned n = 0; n < tail_count; ++n) {
          log_trace("ttl: remove-tail %" PRIu64, serial);
          generate_pair(tail_serial);
          int err = mdbx_del(txn_guard.get(), dbi, &key->value, &data->value);
          if (unlikely(err != MDBX_SUCCESS))
            failure_perror("mdbx_del(tail)", err);
          if (unlikely(!keyvalue_maker.increment(tail_serial, 1)))
            failure("ttl: unexpected key-space overflow on the tail");
        }
      }
    } else {
      log_trace("ttl: purge state");
      db_table_clear(dbi);
      fifo.clear();
    }

    txn_restart(false, false);
    fifo.push_front(std::make_pair(serial, head_count));

    for (unsigned n = 0; n < head_count; ++n) {
      log_trace("ttl: insert-head %" PRIu64, serial);
      generate_pair(serial);
      int err = mdbx_put(txn_guard.get(), dbi, &key->value, &data->value,
                         insert_flags);
      if (unlikely(err != MDBX_SUCCESS))
        failure_perror("mdbx_put(head)", err);

      if (unlikely(!keyvalue_maker.increment(serial, 1)))
        failure("uphill: unexpected key-space overflow");
    }

    txn_end(false);
    report(1);
  }

  if (dbi) {
    if (config.params.drop_table && !mode_readonly()) {
      txn_begin(false);
      db_table_drop(dbi);
      txn_end(false);
    } else
      db_table_close(dbi);
  }
  return true;
}
