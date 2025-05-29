#include "dwarf_def.h"

struct NamedEntry {
    int m_value;
    const char* m_name;
};

static const char* searchNamedEntry(NamedEntry* table, unsigned size, int value) {
    // a table may have some holes, so the search is nto direct
    if (value < size) {
        NamedEntry* start = table + value;
        if (start->m_value == value) {
            return start->m_name;
        }
        if (start->m_value > value) {
            // search backwards
            while (start >= table && start->m_value > value) {
                --start;
            }
        } else {
            // search forward
            while (start < table + size && start->m_value < value) {
                --start;
            }
        }
        if (start >= table && start < table + size && start->m_value == value) {
            return start->m_name;
        }
    }
    return "N/A";
}

#define DECL_NAME_ENTRY(Name) {Name, #Name}

static NamedEntry sTagNameTable[] = {DECL_NAME_ENTRY(DW_TAG_array_type),
                                     DECL_NAME_ENTRY(DW_TAG_class_type),
                                     DECL_NAME_ENTRY(DW_TAG_entry_point),
                                     DECL_NAME_ENTRY(DW_TAG_enumeration_type),
                                     DECL_NAME_ENTRY(DW_TAG_formal_parameter),
                                     DECL_NAME_ENTRY(DW_TAG_imported_declaration),
                                     DECL_NAME_ENTRY(DW_TAG_label),
                                     DECL_NAME_ENTRY(DW_TAG_lexical_block),
                                     DECL_NAME_ENTRY(DW_TAG_member),
                                     DECL_NAME_ENTRY(DW_TAG_pointer_type),
                                     DECL_NAME_ENTRY(DW_TAG_reference_type),
                                     DECL_NAME_ENTRY(DW_TAG_compile_unit),
                                     DECL_NAME_ENTRY(DW_TAG_string_type),
                                     DECL_NAME_ENTRY(DW_TAG_structure_type),
                                     DECL_NAME_ENTRY(DW_TAG_subroutine_type),
                                     DECL_NAME_ENTRY(DW_TAG_typedef),
                                     DECL_NAME_ENTRY(DW_TAG_union_type),
                                     DECL_NAME_ENTRY(DW_TAG_unspecified_parameters),
                                     DECL_NAME_ENTRY(DW_TAG_variant),
                                     DECL_NAME_ENTRY(DW_TAG_common_block),
                                     DECL_NAME_ENTRY(DW_TAG_common_inclusion),
                                     DECL_NAME_ENTRY(DW_TAG_inheritance),
                                     DECL_NAME_ENTRY(DW_TAG_inlined_subroutine),
                                     DECL_NAME_ENTRY(DW_TAG_module),
                                     DECL_NAME_ENTRY(DW_TAG_ptr_to_member_type),
                                     DECL_NAME_ENTRY(DW_TAG_set_type),
                                     DECL_NAME_ENTRY(DW_TAG_subrange_type),
                                     DECL_NAME_ENTRY(DW_TAG_with_stmt),
                                     DECL_NAME_ENTRY(DW_TAG_access_declaration),
                                     DECL_NAME_ENTRY(DW_TAG_base_type),
                                     DECL_NAME_ENTRY(DW_TAG_catch_block),
                                     DECL_NAME_ENTRY(DW_TAG_const_type),
                                     DECL_NAME_ENTRY(DW_TAG_constant),
                                     DECL_NAME_ENTRY(DW_TAG_enumerator),
                                     DECL_NAME_ENTRY(DW_TAG_file_type),
                                     DECL_NAME_ENTRY(DW_TAG_friend),
                                     DECL_NAME_ENTRY(DW_TAG_namelist),
                                     DECL_NAME_ENTRY(DW_TAG_namelist_item),
                                     DECL_NAME_ENTRY(DW_TAG_packed_type),
                                     DECL_NAME_ENTRY(DW_TAG_subprogram),
                                     DECL_NAME_ENTRY(DW_TAG_template_type_parameter),
                                     DECL_NAME_ENTRY(DW_TAG_template_value_parameter),
                                     DECL_NAME_ENTRY(DW_TAG_thrown_type),
                                     DECL_NAME_ENTRY(DW_TAG_try_block),
                                     DECL_NAME_ENTRY(DW_TAG_variant_part),
                                     DECL_NAME_ENTRY(DW_TAG_variable),
                                     DECL_NAME_ENTRY(DW_TAG_volatile_type),
                                     DECL_NAME_ENTRY(DW_TAG_dwarf_procedure),
                                     DECL_NAME_ENTRY(DW_TAG_restrict_type),
                                     DECL_NAME_ENTRY(DW_TAG_interface_type),
                                     DECL_NAME_ENTRY(DW_TAG_namespace),
                                     DECL_NAME_ENTRY(DW_TAG_imported_module),
                                     DECL_NAME_ENTRY(DW_TAG_unspecified_type),
                                     DECL_NAME_ENTRY(DW_TAG_partial_unit),
                                     DECL_NAME_ENTRY(DW_TAG_imported_unit),
                                     DECL_NAME_ENTRY(DW_TAG_condition),
                                     DECL_NAME_ENTRY(DW_TAG_shared_type),
                                     DECL_NAME_ENTRY(DW_TAG_type_unit),
                                     DECL_NAME_ENTRY(DW_TAG_rvalue_reference_type),
                                     DECL_NAME_ENTRY(DW_TAG_template_alias),
                                     DECL_NAME_ENTRY(DW_TAG_coarray_type),
                                     DECL_NAME_ENTRY(DW_TAG_generic_subrange),
                                     DECL_NAME_ENTRY(DW_TAG_dynamic_type),
                                     DECL_NAME_ENTRY(DW_TAG_atomic_type),
                                     DECL_NAME_ENTRY(DW_TAG_call_site),
                                     DECL_NAME_ENTRY(DW_TAG_call_site_parameter),
                                     DECL_NAME_ENTRY(DW_TAG_skeleton_unit),
                                     DECL_NAME_ENTRY(DW_TAG_immutable_type),
                                     {0, nullptr}};

static unsigned sTagNameTableSize = sizeof(sTagNameTable) / sizeof(sTagNameTable[0]);

static NamedEntry sAttNameTable[] = {DECL_NAME_ENTRY(DW_AT_sibling),
                                     DECL_NAME_ENTRY(DW_AT_location),
                                     DECL_NAME_ENTRY(DW_AT_name),
                                     DECL_NAME_ENTRY(DW_AT_ordering),
                                     DECL_NAME_ENTRY(DW_AT_byte_size),
                                     DECL_NAME_ENTRY(DW_AT_bit_offset),
                                     DECL_NAME_ENTRY(DW_AT_bit_size),
                                     DECL_NAME_ENTRY(DW_AT_stmt_list),
                                     DECL_NAME_ENTRY(DW_AT_low_pc),
                                     DECL_NAME_ENTRY(DW_AT_high_pc),
                                     DECL_NAME_ENTRY(DW_AT_language),
                                     DECL_NAME_ENTRY(DW_AT_discr),
                                     DECL_NAME_ENTRY(DW_AT_discr_value),
                                     DECL_NAME_ENTRY(DW_AT_visibility),
                                     DECL_NAME_ENTRY(DW_AT_import),
                                     DECL_NAME_ENTRY(DW_AT_string_length),
                                     DECL_NAME_ENTRY(DW_AT_common_reference),
                                     DECL_NAME_ENTRY(DW_AT_comp_dir),
                                     DECL_NAME_ENTRY(DW_AT_const_value),
                                     DECL_NAME_ENTRY(DW_AT_containing_type),
                                     DECL_NAME_ENTRY(DW_AT_default_value),
                                     DECL_NAME_ENTRY(DW_AT_inline),
                                     DECL_NAME_ENTRY(DW_AT_is_optional),
                                     DECL_NAME_ENTRY(DW_AT_lower_bound),
                                     DECL_NAME_ENTRY(DW_AT_producer),
                                     DECL_NAME_ENTRY(DW_AT_prototyped),
                                     DECL_NAME_ENTRY(DW_AT_return_addr),
                                     DECL_NAME_ENTRY(DW_AT_start_scope),
                                     DECL_NAME_ENTRY(DW_AT_bit_stride),
                                     DECL_NAME_ENTRY(DW_AT_upper_bound),
                                     DECL_NAME_ENTRY(DW_AT_abstract_origin),
                                     DECL_NAME_ENTRY(DW_AT_accessibility),
                                     DECL_NAME_ENTRY(DW_AT_address_class),
                                     DECL_NAME_ENTRY(DW_AT_artificial),
                                     DECL_NAME_ENTRY(DW_AT_base_types),
                                     DECL_NAME_ENTRY(DW_AT_calling_convention),
                                     DECL_NAME_ENTRY(DW_AT_count),
                                     DECL_NAME_ENTRY(DW_AT_data_member_location),
                                     DECL_NAME_ENTRY(DW_AT_decl_column),
                                     DECL_NAME_ENTRY(DW_AT_decl_file),
                                     DECL_NAME_ENTRY(DW_AT_decl_line),
                                     DECL_NAME_ENTRY(DW_AT_declaration),
                                     DECL_NAME_ENTRY(DW_AT_discr_list),
                                     DECL_NAME_ENTRY(DW_AT_encoding),
                                     DECL_NAME_ENTRY(DW_AT_external),
                                     DECL_NAME_ENTRY(DW_AT_frame_base),
                                     DECL_NAME_ENTRY(DW_AT_friend),
                                     DECL_NAME_ENTRY(DW_AT_identifier_case),
                                     DECL_NAME_ENTRY(DW_AT_macro_info),
                                     DECL_NAME_ENTRY(DW_AT_namelist_item),
                                     DECL_NAME_ENTRY(DW_AT_priority),
                                     DECL_NAME_ENTRY(DW_AT_segment),
                                     DECL_NAME_ENTRY(DW_AT_specification),
                                     DECL_NAME_ENTRY(DW_AT_static_link),
                                     DECL_NAME_ENTRY(DW_AT_type),
                                     DECL_NAME_ENTRY(DW_AT_use_location),
                                     DECL_NAME_ENTRY(DW_AT_variable_parameter),
                                     DECL_NAME_ENTRY(DW_AT_virtuality),
                                     DECL_NAME_ENTRY(DW_AT_vtable_elem_location),
                                     DECL_NAME_ENTRY(DW_AT_allocated),
                                     DECL_NAME_ENTRY(DW_AT_associated),
                                     DECL_NAME_ENTRY(DW_AT_data_location),
                                     DECL_NAME_ENTRY(DW_AT_byte_stride),
                                     DECL_NAME_ENTRY(DW_AT_entry_pc),
                                     DECL_NAME_ENTRY(DW_AT_use_UTF8),
                                     DECL_NAME_ENTRY(DW_AT_extension),
                                     DECL_NAME_ENTRY(DW_AT_ranges),
                                     DECL_NAME_ENTRY(DW_AT_trampoline),
                                     DECL_NAME_ENTRY(DW_AT_call_column),
                                     DECL_NAME_ENTRY(DW_AT_call_file),
                                     DECL_NAME_ENTRY(DW_AT_call_line),
                                     DECL_NAME_ENTRY(DW_AT_description),
                                     DECL_NAME_ENTRY(DW_AT_binary_scale),
                                     DECL_NAME_ENTRY(DW_AT_decimal_scale),
                                     DECL_NAME_ENTRY(DW_AT_small),
                                     DECL_NAME_ENTRY(DW_AT_decimal_sign),
                                     DECL_NAME_ENTRY(DW_AT_digit_count),
                                     DECL_NAME_ENTRY(DW_AT_picture_string),
                                     DECL_NAME_ENTRY(DW_AT_mutable),
                                     DECL_NAME_ENTRY(DW_AT_threads_scaled),
                                     DECL_NAME_ENTRY(DW_AT_explicit),
                                     DECL_NAME_ENTRY(DW_AT_object_pointer),
                                     DECL_NAME_ENTRY(DW_AT_endianity),
                                     DECL_NAME_ENTRY(DW_AT_elemental),
                                     DECL_NAME_ENTRY(DW_AT_pure),
                                     DECL_NAME_ENTRY(DW_AT_recursive),
                                     DECL_NAME_ENTRY(DW_AT_signature),
                                     DECL_NAME_ENTRY(DW_AT_main_subprogram),
                                     DECL_NAME_ENTRY(DW_AT_data_bit_offset),
                                     DECL_NAME_ENTRY(DW_AT_const_expr),
                                     DECL_NAME_ENTRY(DW_AT_enum_class),
                                     DECL_NAME_ENTRY(DW_AT_linkage_name),
                                     DECL_NAME_ENTRY(DW_AT_string_length_bit_size),
                                     DECL_NAME_ENTRY(DW_AT_string_length_byte_size),
                                     DECL_NAME_ENTRY(DW_AT_rank),
                                     DECL_NAME_ENTRY(DW_AT_str_offsets_base),
                                     DECL_NAME_ENTRY(DW_AT_addr_base),
                                     DECL_NAME_ENTRY(DW_AT_rnglists_base),
                                     DECL_NAME_ENTRY(DW_AT_dwo_name),
                                     DECL_NAME_ENTRY(DW_AT_reference),
                                     DECL_NAME_ENTRY(DW_AT_rvalue_reference),
                                     DECL_NAME_ENTRY(DW_AT_macros),
                                     DECL_NAME_ENTRY(DW_AT_call_all_calls),
                                     DECL_NAME_ENTRY(DW_AT_call_all_source_calls),
                                     DECL_NAME_ENTRY(DW_AT_call_all_tail_calls),
                                     DECL_NAME_ENTRY(DW_AT_call_return_pc),
                                     DECL_NAME_ENTRY(DW_AT_call_value),
                                     DECL_NAME_ENTRY(DW_AT_call_origin),
                                     DECL_NAME_ENTRY(DW_AT_call_parameter),
                                     DECL_NAME_ENTRY(DW_AT_call_pc),
                                     DECL_NAME_ENTRY(DW_AT_call_tail_call),
                                     DECL_NAME_ENTRY(DW_AT_call_target),
                                     DECL_NAME_ENTRY(DW_AT_call_target_clobbered),
                                     DECL_NAME_ENTRY(DW_AT_call_data_location),
                                     DECL_NAME_ENTRY(DW_AT_call_data_value),
                                     DECL_NAME_ENTRY(DW_AT_noreturn),
                                     DECL_NAME_ENTRY(DW_AT_alignment),
                                     DECL_NAME_ENTRY(DW_AT_export_symbols),
                                     DECL_NAME_ENTRY(DW_AT_deleted),
                                     DECL_NAME_ENTRY(DW_AT_defaulted),
                                     DECL_NAME_ENTRY(DW_AT_loclists_base),
                                     {0, nullptr}};

static unsigned sAttNameTableSize = sizeof(sAttNameTable) / sizeof(sAttNameTable[0]);

static NamedEntry sFormNameTable[] = {DECL_NAME_ENTRY(DW_TAG_array_type),
                                      DECL_NAME_ENTRY(DW_FORM_addr),
                                      DECL_NAME_ENTRY(DW_FORM_block2),
                                      DECL_NAME_ENTRY(DW_FORM_block4),
                                      DECL_NAME_ENTRY(DW_FORM_data2),
                                      DECL_NAME_ENTRY(DW_FORM_data4),
                                      DECL_NAME_ENTRY(DW_FORM_data8),
                                      DECL_NAME_ENTRY(DW_FORM_string),
                                      DECL_NAME_ENTRY(DW_FORM_block),
                                      DECL_NAME_ENTRY(DW_FORM_block1),
                                      DECL_NAME_ENTRY(DW_FORM_data1),
                                      DECL_NAME_ENTRY(DW_FORM_flag),
                                      DECL_NAME_ENTRY(DW_FORM_sdata),
                                      DECL_NAME_ENTRY(DW_FORM_strp),
                                      DECL_NAME_ENTRY(DW_FORM_udata),
                                      DECL_NAME_ENTRY(DW_FORM_ref_addr),
                                      DECL_NAME_ENTRY(DW_FORM_ref1),
                                      DECL_NAME_ENTRY(DW_FORM_ref2),
                                      DECL_NAME_ENTRY(DW_FORM_ref4),
                                      DECL_NAME_ENTRY(DW_FORM_ref8),
                                      DECL_NAME_ENTRY(DW_FORM_ref_udata),
                                      DECL_NAME_ENTRY(DW_FORM_indirect),
                                      DECL_NAME_ENTRY(DW_FORM_sec_offset),
                                      DECL_NAME_ENTRY(DW_FORM_exprloc),
                                      DECL_NAME_ENTRY(DW_FORM_flag_present),
                                      DECL_NAME_ENTRY(DW_FORM_strx),
                                      DECL_NAME_ENTRY(DW_FORM_addrx),
                                      DECL_NAME_ENTRY(DW_FORM_ref_sup4),
                                      DECL_NAME_ENTRY(DW_FORM_strp_sup),
                                      DECL_NAME_ENTRY(DW_FORM_data16),
                                      DECL_NAME_ENTRY(DW_FORM_line_strp),
                                      DECL_NAME_ENTRY(DW_FORM_ref_sig8),
                                      DECL_NAME_ENTRY(DW_FORM_implicit_const),
                                      DECL_NAME_ENTRY(DW_FORM_loclistx),
                                      DECL_NAME_ENTRY(DW_FORM_rnglistx),
                                      DECL_NAME_ENTRY(DW_FORM_ref_sup8),
                                      DECL_NAME_ENTRY(DW_FORM_strx1),
                                      DECL_NAME_ENTRY(DW_FORM_strx2),
                                      DECL_NAME_ENTRY(DW_FORM_strx3),
                                      DECL_NAME_ENTRY(DW_FORM_strx4),
                                      DECL_NAME_ENTRY(DW_FORM_addrx1),
                                      DECL_NAME_ENTRY(DW_FORM_addrx2),
                                      DECL_NAME_ENTRY(DW_FORM_addrx3),
                                      DECL_NAME_ENTRY(DW_FORM_addrx4),
                                      {0, nullptr}};

static unsigned sFormNameTableSize = sizeof(sFormNameTable) / sizeof(sFormNameTable[0]);

extern const char* getDwarfTagName(int tagName) {
    if (tagName == DW_TAG_lo_user) {
        return "DW_TAG_lo_user";
    }
    if (tagName == DW_TAG_hi_user) {
        return "DW_TAG_hi_user";
    }
    return searchNamedEntry(sTagNameTable, sTagNameTableSize, tagName);
}

extern const char* getDwarfAttributeName(int attName) {
    if (attName == DW_AT_lo_user) {
        return "DW_AT_lo_user";
    }
    if (attName == DW_AT_hi_user) {
        return "DW_AT_hi_user";
    }
    return searchNamedEntry(sAttNameTable, sAttNameTableSize, attName);
}

extern const char* getDwarfFormName(int formName) {
    return searchNamedEntry(sFormNameTable, sFormNameTableSize, formName);
}