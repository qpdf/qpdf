// Copyright (c) 2005-2021 Jay Berkenbilt
// Copyright (c) 2022-2026 Jay Berkenbilt and Manfred Holger
//
// This file is part of qpdf.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the License
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
// or implied. See the License for the specific language governing permissions and limitations under
// the License.
//
// Versions of qpdf prior to version 7 were released under the terms of version 2.0 of the Artistic
// License. At your option, you may continue to consider qpdf to be licensed under those terms.
// Please see the manual for additional information.

#ifndef GLOBAL_HH
#define GLOBAL_HH

#include <qpdf/Constants.h>

#include <qpdf/QUtil.hh>
#include <qpdf/qpdf-c.h>

#include <cstdint>

namespace qpdf::global
{
    /// Helper function to translate result codes into C++ exceptions - for qpdf internal use only.
    inline void
    handle_result(qpdf_result_e result)
    {
        if (result != qpdf_r_ok) {
            QUtil::handle_result_code(result, "qpdf::global");
        }
    }

    /// Helper function to wrap calls to qpdf_global_get_uint32 - for qpdf internal use only.
    inline uint32_t
    get_uint32(qpdf_param_e param)
    {
        uint32_t value;
        handle_result(qpdf_global_get_uint32(param, &value));
        return value;
    }

    /// Helper function to wrap calls to qpdf_global_set_uint32 - for qpdf internal use only.
    inline void
    set_uint32(qpdf_param_e param, uint32_t value)
    {
        handle_result(qpdf_global_set_uint32(param, value));
    }

    /// @brief Retrieves the number of limit errors.
    ///
    /// Returns the number of times a global limit was exceeded. This item is read only.
    ///
    /// @return The number of limit errors.
    ///
    /// @since 12.3
    uint32_t inline limit_errors()
    {
        return get_uint32(qpdf_p_limit_errors);
    }

    namespace options
    {
        /// @brief  Retrieves whether inspection mode is set.
        ///
        /// @return True if inspection mode is set.
        ///
        /// @since 12.3
        bool inline inspection_mode()
        {
            return get_uint32(qpdf_p_inspection_mode) != 0;
        }

        /// @brief  Set inspection mode if `true` is passed.
        ///
        /// This function enables restrictive inspection mode if `true` is passed. Inspection mode
        /// must be enabled before a QPDF object is created. By default inspection mode is off.
        /// Calling `inspection_mode(false)` is not supported and currently is a no-op.
        ///
        /// @param value A boolean indicating whether to enable (true) inspection mode.
        ///
        /// @since 12.3
        void inline inspection_mode(bool value)
        {
            set_uint32(qpdf_p_inspection_mode, value ? QPDF_TRUE : QPDF_FALSE);
        }

        /// @brief  Retrieves whether default limits are enabled.
        ///
        /// @return True if default limits are enabled.
        ///
        /// @since 12.3
        bool inline default_limits()
        {
            return get_uint32(qpdf_p_default_limits) != 0;
        }

        /// @brief  Disable all optional default limits if `false` is passed.
        ///
        /// This function disables all optional default limits if `false` is passed. Once default
        /// values have been disabled they cannot be re-enabled. Passing `true` has no effect. This
        /// function will leave any limits that have been explicitly set unchanged. Some limits,
        /// such as limits imposed to avoid stack overflows, cannot be disabled but can be changed.
        ///
        /// @param value A boolean indicating whether to disable (false) the default limits.
        ///
        /// @since 12.3
        void inline default_limits(bool value)
        {
            set_uint32(qpdf_p_default_limits, value ? QPDF_TRUE : QPDF_FALSE);
        }

    } // namespace options

    namespace limits
    {
        /// @brief Retrieves the maximum nesting level while parsing objects.
        ///
        /// @return The maximum nesting level while parsing objects.
        ///
        /// @note The maximum nesting level cannot be disabled by calling `default_limit(false)`.
        ///
        /// @since 12.3
        uint32_t inline parser_max_nesting()
        {
            return get_uint32(qpdf_p_parser_max_nesting);
        }

        /// @brief Sets the maximum nesting level while parsing objects.
        ///
        /// @param value The maximum nesting level to set.
        ///
        /// @note The maximum nesting level cannot be disabled by calling `default_limit(false)`.
        ///
        /// @since 12.3
        void inline parser_max_nesting(uint32_t value)
        {
            set_uint32(qpdf_p_parser_max_nesting, value);
        }

        /// @brief Retrieves the maximum number of errors allowed while parsing objects.
        ///
        /// A value of 0 means that there is no maximum imposed.
        ///
        /// @return The maximum number of errors allowed while parsing objects.
        ///
        /// @since 12.3
        uint32_t inline parser_max_errors()
        {
            return get_uint32(qpdf_p_parser_max_errors);
        }

        /// Sets the maximum number of errors allowed while parsing objects.
        ///
        /// A value of 0 means that there is no maximum imposed.
        ///
        /// @param value The maximum number of errors allowed while parsing objects to set.
        ///
        /// @since 12.3
        void inline parser_max_errors(uint32_t value)
        {
            set_uint32(qpdf_p_parser_max_errors, value);
        }

        /// @brief Retrieves the maximum number of top-level objects allowed in a container while
        ///        parsing.
        ///
        /// The limit applies when the PDF document's xref table is undamaged and the object itself
        /// can be parsed without errors. The default limit is 4,294,967,295.
        ///
        /// @return The maximum number of top-level objects allowed in a container while parsing
        ///         objects.
        ///
        /// @since 12.3
        uint32_t inline parser_max_container_size()
        {
            return get_uint32(qpdf_p_parser_max_container_size);
        }

        /// @brief Sets the maximum number of top-level objects allowed in a container while
        ///        parsing.
        ///
        /// The limit applies when the PDF document's xref table is undamaged and the object itself
        /// can be parsed without errors. The default limit is 4,294,967,295.
        ///
        /// @param value  The maximum number of top-level objects allowed in a container while
        ///               parsing objects to set.
        ///
        /// @since 12.3
        void inline parser_max_container_size(uint32_t value)
        {
            set_uint32(qpdf_p_parser_max_container_size, value);
        }

        /// @brief Retrieves the maximum number of top-level objects allowed in a container while
        ///        parsing objects.
        ///
        /// The limit applies when the PDF document's xref table is damaged or the object itself is
        /// damaged. The limit also applies when parsing xref streams. The default limit is 5,000.
        ///
        /// @return The maximum number of top-level objects allowed in a container while parsing
        ///         objects.
        ///
        /// @since 12.3
        uint32_t inline parser_max_container_size_damaged()
        {
            return get_uint32(qpdf_p_parser_max_container_size_damaged);
        }

        /// @brief Sets the maximum number of top-level objects allowed in a container while
        ///        parsing.
        ///
        /// The limit applies when the PDF document's xref table is damaged or the object itself is
        /// damaged. The limit also applies when parsing trailer dictionaries and xref streams. The
        /// default limit is 5,000.
        ///
        /// @param value  The maximum number of top-level objects allowed in a container while
        ///               parsing objects to set.
        ///
        /// @since 12.3
        void inline parser_max_container_size_damaged(uint32_t value)
        {
            set_uint32(qpdf_p_parser_max_container_size_damaged, value);
        }

        /// @brief Retrieves the maximum number of filters allowed when filtering streams.
        ///
        /// An excessive number of stream filters is usually a sign that a file is damaged or
        /// specially constructed. If the maximum is exceeded for a stream the stream is treated as
        /// unfilterable. The default maximum is 25.
        ///
        /// @return The maximum number of filters allowed when filtering streams.
        ///
        /// @since 12.3
        uint32_t inline max_stream_filters()
        {
            return get_uint32(qpdf_p_max_stream_filters);
        }

        /// @brief Sets the maximum number of filters allowed when filtering streams.
        ///
        /// An excessive number of stream filters is usually a sign that a file is damaged or
        /// specially constructed. If the maximum is exceeded for a stream the stream is treated as
        /// unfilterable. The default maximum is 25.
        ///
        /// @param value  The maximum number of filters allowed when filtering streams to set.
        ///
        /// @since 12.3
        void inline max_stream_filters(uint32_t value)
        {
            set_uint32(qpdf_p_max_stream_filters, value);
        }
    } // namespace limits

} // namespace qpdf::global

#endif // GLOBAL_HH
