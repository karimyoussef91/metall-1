// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAILL_SEGMENT_STORAGE_PRIVATEER_SEGMENT_STORAGE_HPP
#define METALL_DETAILL_SEGMENT_STORAGE_PRIVATEER_SEGMENT_STORAGE_HPP

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif


#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>


#include <privateer/privateer.hpp>

#include <string>
#include <iostream>
#include <cassert>
#include <metall/detail/file.hpp>
#include <metall/detail/mmap.hpp>
#include <metall/detail/utilities.hpp>
#include <metall/detail/time.hpp>


namespace metall {
namespace kernel {

namespace {
namespace mdtl = metall::mtlldetail;
}

template <typename different_type, typename size_type>
class privateer_segment_storage {

 public:
  // -------------------------------------------------------------------------------- //
  // Constructor & assign operator
  // -------------------------------------------------------------------------------- //
  privateer_segment_storage()
      : m_system_page_size(0),
        m_vm_region_size(0),
        m_current_segment_size(0),
        m_segment(nullptr),
        m_base_path(),
        m_read_only(),
        m_free_file_space(true) {

    priv_load_system_page_size();
  }

  ~privateer_segment_storage() {
    priv_sync_segment(true);
    destroy();
  }

  privateer_segment_storage(const privateer_segment_storage &) = delete;
  privateer_segment_storage &operator=(const privateer_segment_storage &) = delete;

  privateer_segment_storage(privateer_segment_storage &&other) noexcept:
      m_system_page_size(other.m_system_page_size),
      m_vm_region_size(other.m_vm_region_size),
      m_current_segment_size(other.m_current_segment_size),
      m_segment(other.m_segment),
      m_base_path(other.m_base_path),
      m_read_only(other.m_read_only),
      m_free_file_space(other.m_free_file_space) {
    other.priv_reset();
  }

  privateer_segment_storage &operator=(privateer_segment_storage &&other) noexcept {
    m_system_page_size = other.m_system_page_size;
    m_vm_region_size = other.m_vm_region_size;
    m_current_segment_size = other.m_current_segment_size;
    m_segment = other.m_segment;
    m_base_path = other.m_base_path;
    m_read_only = other.m_read_only;
    m_free_file_space = other.m_free_file_space;

    other.priv_reset();

    return (*this);
  }

  // -------------------------------------------------------------------------------- //
  // Public methods
  // -------------------------------------------------------------------------------- //
  /// \brief Check if there is a file that can be opened
  static bool openable(const std::string &base_path) {
    // return mdtl::file_exist(priv_make_file_name(base_path));
    return mdtl::file_exist(base_path);
  }

  /// \brief Gets the size of an existing segment.
  /// This is a static version of size() method.
  static size_type get_size(const std::string &base_path) {
    // const auto directory_name = priv_make_file_name(base_path);
    const auto directory_name = base_path;
    std::string version_path = directory_name + "/version_metadata";
    return Privateer::version_size(version_path); // TODO: Implement Static get_size in Privateer
  }

  /// \brief Copies segment to another location.
  /// \param source_path A path to a source segment.
  /// \param destination_path A destination path.
  /// \param clone If true, uses clone (reflink) for copying files.
  /// \param max_num_threads The maximum number of threads to use.
  /// If <= 0 is given, the value is automatically determined.
  /// \return Return true if success; otherwise, false.
  static bool copy(const std::string &source_path,
                   const std::string &destination_path,
                   const bool clone,
                   const int max_num_threads) {
    std::string destination_privateer_metadata_path = destination_path + "/version_metadata";
    if (!mdtl::directory_exist(destination_privateer_metadata_path)) {
      if (!mdtl::create_directory(destination_privateer_metadata_path)) {
        std::string s("Cannot create a directory: " + destination_privateer_metadata_path);
        logger::out(logger::level::critical, __FILE__, __LINE__, s.c_str());
      }
    }

    std::string source_privateer_metadata_path = source_path + "/version_metadata";

    if (clone) {
      std::string s("Clone: " + source_path);
      logger::out(logger::level::info, __FILE__, __LINE__, s.c_str());
      return mdtl::clone_files_in_directory_in_parallel(source_privateer_metadata_path, destination_privateer_metadata_path, max_num_threads);
    } else {
      std::string s("Copy: " + source_path);
      logger::out(logger::level::info, __FILE__, __LINE__, s.c_str());
      return mdtl::copy_files_in_directory_in_parallel(source_privateer_metadata_path, destination_privateer_metadata_path, max_num_threads);
    }
    assert(false);
    return false;
  }


  /// \brief {Creates a new segment by mapping file(s) to the given VM address.
  /// \param base_path A path to create a datastore.
  /// \param vm_region_size The size of the VM region.
  /// \param vm_region The address of the VM region.
  /// \param initial_segment_size_hint Not used.
  /// \return Returns true on success; otherwise, false.
  bool create(const std::string &base_path,
              const size_type vm_region_size,
              void *const vm_region,
              [[maybe_unused]] const size_type initial_segment_size_hint) {
    assert(!priv_inited());

    // TODO: align those values to the page size instead of aborting
    if (vm_region_size % page_size() != 0 || (uint64_t)vm_region % page_size() != 0) {
      std::cerr << "Invalid argument to crete application data segment" << std::endl;
      std::abort();
    }

    m_base_path = base_path;
    m_vm_region_size = vm_region_size;
    m_segment = vm_region;
    m_read_only = false;

    const auto segment_size = vm_region_size;
    if (!priv_create_and_map_file(m_base_path, segment_size, m_segment)) {
      priv_reset();
      return false;
    }

    priv_test_file_space_free(base_path);

    return true;
  }

  /// \brief Opens an existing Metall datastore.
  /// \param base_path The path to datastore.
  /// \param vm_region_size The size of the VM region.
  /// \param vm_region The address of the VM region.
  /// \param read_only If this option is true, opens the datastore with read only mode.
  /// \return Returns true on success; otherwise, false.
  bool open(const std::string &base_path, const size_type vm_region_size, void *const vm_region, const bool read_only) {
    assert(!priv_inited());
    // TODO: align those values to pge size
    if (vm_region_size % page_size() != 0 || (uint64_t)vm_region % page_size() != 0) {
      std::cerr << "Invalid argument to open segment" << std::endl;
      std::abort(); // Fatal error
    }
    m_base_path = base_path;
    m_vm_region_size = vm_region_size;
    m_segment = vm_region;
    m_read_only = read_only;

    const auto file_name = m_base_path;// priv_make_file_name(m_base_path);
    if (!mdtl::file_exist(file_name)) {
      std::cerr << "Segment file does not exist" << std::endl;
      return false;
    }

    if (!priv_map_file_open(file_name, static_cast<char *>(m_segment), read_only)) { // , store)) {
      std::abort(); // Fatal error
    }


    if (!read_only) {
      priv_test_file_space_free(base_path);
    }

    return true;
  }

  /// \brief This function does nothing in this implementation.
  /// \param new_segment_size Not used.
  /// \return Always returns true.
  bool extend(const size_type request_size) {
    assert(priv_inited());

    if (m_read_only) {
      return false;
    }

    if (request_size > m_vm_region_size) {
      std::cerr << "Requested segment size is bigger than the reserved VM size" << std::endl;
      return false;
    }

    bool privateer_extend_status = privateer->resize(request_size);
    if (privateer_extend_status){
      m_current_segment_size = privateer->current_size();
      return true;
    }

    return false;
  }

  /// \brief Destroy (unmap) the segment.
  void destroy() {
    priv_destroy_segment();
  }

  /// \brief Syncs the segment (files) with the storage.
  /// \param sync Not used.
  void sync(const bool sync) {
    priv_sync_segment(sync);
  }

  /// \brief This function does nothing.
  /// \param offset Not used.
  /// \param nbytes Not used.
  void free_region(const different_type offset, const size_type nbytes) {
    priv_free_region(offset, nbytes);
  }

  /// \brief Returns the address of the segment.
  /// \return The address of the segment.
  void *get_segment() const {
    return m_segment;
  }

  /// \brief Returns the size of the segment.
  /// \return The size of the segment.
  size_type size() const {
    return m_current_segment_size;
  }

  size_type page_size() const {
    return m_system_page_size;
  }

  /// \brief Returns whether the segment is read only or not.
  /// \return Returns true if it is read only; otherwise, false.
  bool read_only() const {
    return m_read_only;
  }

 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //

  // -------------------------------------------------------------------------------- //
  // Private methods (not designed to be used by the base class)
  // -------------------------------------------------------------------------------- //
  static std::string priv_make_file_name(const std::string &base_path) {
    return base_path + "_privateer_datastore";
  }

  void priv_reset() {
    m_system_page_size = 0;
    m_vm_region_size = 0;
    m_current_segment_size = 0;
    m_segment = nullptr;
    // m_read_only = false;
  }

  bool priv_inited() const {
    return (m_system_page_size > 0 && m_vm_region_size > 0 && m_current_segment_size > 0 && m_segment
        && !m_base_path.empty());
  }

  bool priv_create_and_map_file(const std::string &base_path,
                                const size_type file_size,
                                void *const addr) {
    assert(!m_segment || static_cast<char *>(m_segment) + m_current_segment_size <= addr);

    const std::string file_name = base_path;// priv_make_file_name(base_path);
    if (!priv_map_file_create(file_name, file_size, addr)) {
      return false;
    }
    return true;
  }

  bool priv_map_file_create(const std::string &path, const size_type file_size, void *const addr) {
    assert(!path.empty());
    assert(file_size > 0);
    assert(addr);

    std::string blocks_path = path + "/blocks";
    std::string version_path = path + "/version_metadata";
    // create versions directory
    if (!mdtl::create_directory(path)) {
      std::string s("Failed to create directory: " + version_path);
      logger::out(logger::level::critical, __FILE__, __LINE__, s.c_str());
      return false;
    }

    // init Privateer object and get data
    privateer = new Privateer(addr, blocks_path.c_str(), version_path.c_str(), file_size);
    m_current_segment_size = privateer->current_size();
    return true;
  }

  bool priv_map_file_open(const std::string &path, void *const addr, const bool read_only){
    assert(!path.empty());
    assert(addr);

    // MEMO: one of the following options does not work on /tmp?

    std::string version_path = path + "/version_metadata";
    privateer = new Privateer(addr, version_path.c_str(), read_only);
    m_current_segment_size = privateer->current_size();
    return true;
  }


  void priv_unmap_file() {

    const auto file_name = m_base_path;// priv_make_file_name(m_base_path);
    assert(mdtl::file_exist(file_name));
    m_current_segment_size = 0;
    delete privateer;
  }

  void priv_destroy_segment() {
    if (!priv_inited()) return;

    priv_unmap_file();

    priv_reset();
  }

  void priv_sync_segment([[maybe_unused]] const bool sync) {
    if (!priv_inited() || m_read_only) return;

    privateer->msync();
  }

  // MEMO: Privateer cannot free file region
  bool priv_free_region(const different_type offset, const size_type nbytes) {
    if (!priv_inited() || m_read_only) return false;

    if (offset + nbytes > m_current_segment_size) return false;

    return true;
  }

  bool priv_load_system_page_size() {
    m_system_page_size = mdtl::get_page_size();
    if (m_system_page_size == -1) {
      logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to get system pagesize");
      return false;
    }
    return true;
  }

  void priv_test_file_space_free(const std::string &) {
    m_free_file_space = false;
  }

  // -------------------------------------------------------------------------------- //
  // Private fields
  // -------------------------------------------------------------------------------- //
  ssize_t m_system_page_size{0};
  size_type m_vm_region_size{0};
  size_type m_current_segment_size{0};
  void *m_segment{nullptr};
  std::string m_base_path;
  bool m_read_only;
  bool m_free_file_space{true};
  mutable Privateer* privateer;
};

} // namespace kernel
} // namespace metall

#endif //METALL_DETAILL_SEGMENT_STORAGE_PRIVATEER_SEGMENT_STORAGE_HPP