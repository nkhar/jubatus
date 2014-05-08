// Jubatus: Online machine learning framework for distributed environment
// Copyright (C) 2011 Preferred Infrastructure and Nippon Telegraph and Telephone Corporation.
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License version 2.1 as published by the Free Software Foundation.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

#include "lsh.hpp"

#include <cmath>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>
#include "../common/exception.hpp"
#include "../common/hash.hpp"
#include "lsh_util.hpp"

using std::pair;
using std::string;
using std::vector;
using jubatus::core::storage::bit_vector;

namespace jubatus {
namespace core {
namespace recommender {

static const uint64_t DEFAULT_BASE_NUM = 64;  // should be in config

lsh::config::config()
    : hash_num(DEFAULT_BASE_NUM) {
}

lsh::lsh(uint64_t base_num)
    : base_num_(base_num) {
  if (!(1 <= base_num)) {
    throw JUBATUS_EXCEPTION(
        common::invalid_parameter("1 <= base_num"));
  }
  initialize_model();
}

lsh::lsh(const config& config)
    : base_num_(config.hash_num) {

  if (!(1 <= config.hash_num)) {
    throw JUBATUS_EXCEPTION(
        common::invalid_parameter("1 <= hash_num"));
  }

  initialize_model();
}

lsh::lsh()
    : base_num_(DEFAULT_BASE_NUM) {
  initialize_model();
}

lsh::~lsh() {
}

void lsh::similar_row(
    const common::sfv_t& query,
    vector<pair<string, float> >& ids,
    size_t ret_num) const {
  ids.clear();
  if (ret_num == 0) {
    return;
  }

  bit_vector query_bv;
  calc_lsh_values(query, query_bv);
  mixable_storage_->get_model()->similar_row(query_bv, ids, ret_num);
}

void lsh::neighbor_row(
    const common::sfv_t& query,
    vector<pair<string, float> >& ids,
    size_t ret_num) const {
  similar_row(query, ids, ret_num);
  for (size_t i = 0; i < ids.size(); ++i) {
    ids[i].second = 1 - ids[i].second;
  }
}

void lsh::clear() {
  orig_->get_model()->clear();
  jubatus::util::data::unordered_map<std::string, std::vector<float> >()
    .swap(column2baseval_);
  mixable_storage_->get_model()->clear();
}

void lsh::clear_row(const string& id) {
  orig_->get_model()->remove_row(id);
  mixable_storage_->get_model()->remove_row(id);
}

void lsh::calc_lsh_values(const common::sfv_t& sfv, bit_vector& bv) const {
  const_cast<lsh*>(this)->generate_column_bases(sfv);

  vector<float> lsh_vals;
  prod_invert_and_vector(column2baseval_, sfv, base_num_, lsh_vals);
  set_bit_vector(lsh_vals, bv);
}

void lsh::generate_column_bases(const common::sfv_t& sfv) {
  for (size_t i = 0; i < sfv.size(); ++i) {
    generate_column_base(sfv[i].first);
  }
}

void lsh::generate_column_base(const string& column) {
  if (column2baseval_.count(column) != 0) {
    return;
  }
  const uint32_t seed = common::hash_util::calc_string_hash(column);
  generate_random_vector(base_num_, seed, column2baseval_[column]);
}

void lsh::update_row(const string& id, const sfv_diff_t& diff) {
  generate_column_bases(diff);
  core::storage::sparse_matrix_storage_mixable::model_ptr orig =
      orig_->get_model();
  orig->set_row(id, diff);
  common::sfv_t row;
  orig->get_row(id, row);
  bit_vector bv;
  calc_lsh_values(row, bv);
  mixable_storage_->get_model()->set_row(id, bv);
}

void lsh::get_all_row_ids(std::vector<std::string>& ids) const {
  mixable_storage_->get_model()->get_all_row_ids(ids);
}

string lsh::type() const {
  return string("lsh");
}

void lsh::register_mixables_to_holder(framework::mixable_holder& holder) const {
  holder.register_mixable(orig_);
  holder.register_mixable(mixable_storage_);
}

void lsh::initialize_model() {
  mixable_storage_.reset(new storage::mixable_bit_index_storage);
  mixable_storage_->set_model(storage::mixable_bit_index_storage::model_ptr(
      new storage::bit_index_storage));
}

}  // namespace recommender
}  // namespace core
}  // namespace jubatus
