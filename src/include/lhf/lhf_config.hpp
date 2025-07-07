/**
 * @file lhf_config.hpp
 * @brief Any configurable constants go into this file.
 */

#ifndef LHF_CONFIG_HPP
#define LHF_CONFIG_HPP

#define LHF_SORTED_VECTOR_BINARY_SEARCH_THRESHOLD 12
#define LHF_DEFAULT_BLOCK_SIZE (1 << 5)
#define LHF_DEFAULT_BLOCK_MASK (LHF_DEFAULT_BLOCK_SIZE - 1)
#define LHF_DISABLE_INTERNAL_INTEGRITY_CHECK true

#endif