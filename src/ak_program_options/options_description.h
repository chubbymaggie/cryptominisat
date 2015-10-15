/*
 * ak_program_options
 *
 * Copyright (c) 2015, Axel Kemper. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.0 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301  USA
 *
 * Package ak_program_options was derived from boost::program_options
 * as a simplified and restricted package suitable for cryptominisat.
 * Original copyright:
 * Copyright Vladimir Prus 2002-2004.
 * Distributed under the Boost Software License, Version 1.0.
 * (See http://www.boost.org/LICENSE_1_0.txt)
 */

#ifndef OPTIONS_DESCRIPTION_H_INCLUDED
#define OPTIONS_DESCRIPTION_H_INCLUDED

#include <string>
#include <vector>

#include "errors.h"
#include "scan_arguments.h"
#include "option_description.h"
#include "value_semantic.h"

namespace ak_program_options {

    class options_description_easy_init;

    class options_description {
    private:
        std::string m_caption;

        // Data organization is chosen because:
        // - there could be two names for one option
        // - option_add_proxy needs to know the last added option
        std::vector<option_description *> m_options;

        std::vector<options_description *> m_groups;
        std::vector<bool> m_belong_to_group;

        unsigned m_line_length = 80;
        unsigned m_min_description_length = 20;

    public:
        options_description() {};
        //  FIXME:
        //  not really sure if the m_options vector
        //  has to be cleaned up at destruction time
        ~options_description() {}; 
        options_description(const std::string& caption) { m_caption = caption;  };

        void add(option_description *desc);
        options_description &add(const options_description& desc);
        options_description_easy_init add_options();

        unsigned get_option_column_width() const;

        /** Produces a human readable output of 'desc', listing options,
        their descriptions and allowed parameters. Other options_description
        instances previously passed to add will be output separately. */
        friend std::ostream& operator<<(std::ostream& os,
            const options_description& desc);

        /** Outputs 'desc' to the specified stream, calling 'f' to output each
        option_description element. */
        void print(std::ostream& os, unsigned width = 0) const;

        /** return all option_descriptions as vector */
        std::vector<option_description *> options() const;

        const option_description *findById(int id) const;
        const option_description *findByName(std::string name) const;
    };

    /** Class which provides convenient creation syntax to option_description.
    */
    class options_description_easy_init {
    public:
        options_description_easy_init(options_description* owner) {
            m_owner = owner;
        };

        options_description_easy_init&
            operator()(const char *name,
                const char *description);

        options_description_easy_init&
            operator()(const char *name,
                const value_semantic *s);

        options_description_easy_init&
            operator()(const char* name,
                const value_semantic *s,
                const char *description);

    private:
        options_description* m_owner;
    };

}

#endif
