// Copyright 2025 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <string>
#include <filesystem>

#include <metall/metall.hpp>
#include <metall/container/string.hpp>
#include <metall/detail/time.hpp>
#include <metall/utility/open_mp.hpp>

#include <faker-cxx/faker.h>

namespace mdtl = metall::mtlldetail;

int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <path> <num_chars_to_generate>"
              << std::endl;
    return EXIT_FAILURE;
  }

  std::filesystem::path path = argv[1];
  std::size_t num_chars_to_generate = std::stoull(argv[2]);

  std::unique_ptr<metall::manager> manager(
      new metall::manager(metall::create_only, path));

  const auto ingest_start_time = mdtl::elapsed_time_sec();
  OMP_DIRECTIVE(parallel) {
    const int num_threads = metall::utility::omp::get_num_threads();

    std::size_t n = num_chars_to_generate / num_threads;
    std::size_t cnt = 0;
    while (cnt < n) {
      const auto id = faker::string::uuidV4();
      const auto email = faker::internet::email();
      const auto city = faker::location::city();
      const auto streetAddress = faker::location::streetAddress();

      manager->construct<metall::container::string>(metall::anonymous_instance)(
          id, manager->get_allocator());
      cnt += id.size();

      manager->construct<metall::container::string>(metall::anonymous_instance)(
          email, manager->get_allocator());
      cnt += email.size();

      manager->construct<metall::container::string>(metall::anonymous_instance)(
          city, manager->get_allocator());
      cnt += city.size();

      manager->construct<metall::container::string>(metall::anonymous_instance)(
          streetAddress, manager->get_allocator());
      cnt += streetAddress.size();
    }
  }
  const auto ingest_end_time = mdtl::elapsed_time_sec(ingest_start_time);
  std::cout << "Ingested " << num_chars_to_generate << " characters in "
            << ingest_end_time << " seconds." << std::endl;

  const auto metall_sync_start_time = mdtl::elapsed_time_sec();
  manager->flush();
  const auto metall_sync_end_time =
      mdtl::elapsed_time_sec(metall_sync_start_time);
  std::cout << "Closed Metall manager in " << metall_sync_end_time
            << " seconds." << std::endl;

  const auto metall_close_start_time = mdtl::elapsed_time_sec();
  manager.reset(nullptr);
  const auto metall_close_end_time =
      mdtl::elapsed_time_sec(metall_close_start_time);
  std::cout << "Closed Metall manager in " << metall_close_end_time
            << " seconds." << std::endl;

  return EXIT_SUCCESS;
}