#ifndef __DWARF_DEF_H__
#define __DWARF_DEF_H__

// tags
#define DW_TAG_array_type 0x01
#define DW_TAG_class_type 0x02
#define DW_TAG_entry_point 0x03
#define DW_TAG_enumeration_type 0x04
#define DW_TAG_formal_parameter 0x05
#define DW_TAG_imported_declaration 0x08
#define DW_TAG_label 0x0a
#define DW_TAG_lexical_block 0x0b
#define DW_TAG_member 0x0d
#define DW_TAG_pointer_type 0x0f
#define DW_TAG_reference_type 0x10
#define DW_TAG_compile_unit 0x11
#define DW_TAG_string_type 0x12
#define DW_TAG_structure_type 0x13
#define DW_TAG_subroutine_type 0x15
#define DW_TAG_typedef 0x16
#define DW_TAG_union_type 0x17
#define DW_TAG_unspecified_parameters 0x18
#define DW_TAG_variant 0x19
#define DW_TAG_common_block 0x1a
#define DW_TAG_common_inclusion 0x1b
#define DW_TAG_inheritance 0x1c
#define DW_TAG_inlined_subroutine 0x1d
#define DW_TAG_module 0x1e
#define DW_TAG_ptr_to_member_type 0x1f
#define DW_TAG_set_type 0x20
#define DW_TAG_subrange_type 0x21
#define DW_TAG_with_stmt 0x22
#define DW_TAG_access_declaration 0x23
#define DW_TAG_base_type 0x24
#define DW_TAG_catch_block 0x25
#define DW_TAG_const_type 0x26
#define DW_TAG_constant 0x27
#define DW_TAG_enumerator 0x28
#define DW_TAG_file_type 0x29
#define DW_TAG_friend 0x2a
#define DW_TAG_namelist 0x2b
#define DW_TAG_namelist_item 0x2c
#define DW_TAG_packed_type 0x2d
#define DW_TAG_subprogram 0x2e
#define DW_TAG_template_type_parameter 0x2f
#define DW_TAG_template_value_parameter 0x30
#define DW_TAG_thrown_type 0x31
#define DW_TAG_try_block 0x32
#define DW_TAG_variant_part 0x33
#define DW_TAG_variable 0x34
#define DW_TAG_volatile_type 0x35
#define DW_TAG_dwarf_procedure 0x36
#define DW_TAG_restrict_type 0x37
#define DW_TAG_interface_type 0x38
#define DW_TAG_namespace 0x39
#define DW_TAG_imported_module 0x3a
#define DW_TAG_unspecified_type 0x3b
#define DW_TAG_partial_unit 0x3c
#define DW_TAG_imported_unit 0x3d
#define DW_TAG_condition 0x3f
#define DW_TAG_shared_type 0x40

// version 5 tags
#define DW_TAG_type_unit 0x41
#define DW_TAG_rvalue_reference_type 0x42
#define DW_TAG_template_alias 0x43
#define DW_TAG_coarray_type 0x44
#define DW_TAG_generic_subrange 0x45
#define DW_TAG_dynamic_type 0x46
#define DW_TAG_atomic_type 0x47
#define DW_TAG_call_site 0x48
#define DW_TAG_call_site_parameter 0x49
#define DW_TAG_skeleton_unit 0x4a
#define DW_TAG_immutable_type 0x4b

// normal attributes
#define DW_TAG_lo_user 0x4080
#define DW_TAG_hi_user 0xffff

// children
#define DW_CHILDREN_no 0x00
#define DW_CHILDREN_yes 0x01

// attributes
#define DW_AT_sibling 0x01               // class reference
#define DW_AT_location 0x02              // class block, loclistptr
#define DW_AT_name 0x03                  // class string
#define DW_AT_ordering 0x09              // class constant
#define DW_AT_byte_size 0x0b             // class block, constant, reference
#define DW_AT_bit_offset 0x0c            // class block, constant, reference - up to version 3
#define DW_AT_bit_size 0x0d              // class block, constant, reference
#define DW_AT_stmt_list 0x10             // class lineptr
#define DW_AT_low_pc 0x11                // class address
#define DW_AT_high_pc 0x12               // class address
#define DW_AT_language 0x13              // class constant
#define DW_AT_discr 0x15                 // class reference
#define DW_AT_discr_value 0x16           // class constant
#define DW_AT_visibility 0x17            // class constant
#define DW_AT_import 0x18                // class reference
#define DW_AT_string_length 0x19         // class block, loclistptr
#define DW_AT_common_reference 0x1a      // class reference
#define DW_AT_comp_dir 0x1b              // class string
#define DW_AT_const_value 0x1c           // class block, constant, string
#define DW_AT_containing_type 0x1d       // class reference
#define DW_AT_default_value 0x1e         // class reference
#define DW_AT_inline 0x20                // class constant
#define DW_AT_is_optional 0x21           // class flag
#define DW_AT_lower_bound 0x22           // class block, constant, reference
#define DW_AT_producer 0x25              // class string
#define DW_AT_prototyped 0x27            // class flag
#define DW_AT_return_addr 0x2a           // class block, loclistptr
#define DW_AT_start_scope 0x2c           // class constant
#define DW_AT_bit_stride 0x2e            // class constant
#define DW_AT_upper_bound 0x2f           // class block, constant, reference
#define DW_AT_abstract_origin 0x31       // class reference
#define DW_AT_accessibility 0x32         // class constant
#define DW_AT_address_class 0x33         // class constant
#define DW_AT_artificial 0x34            // class flag
#define DW_AT_base_types 0x35            // class reference
#define DW_AT_calling_convention 0x36    // class constant
#define DW_AT_count 0x37                 // class block, constant, reference
#define DW_AT_data_member_location 0x38  // class block, constant, loclistptr
#define DW_AT_decl_column 0x39           // class constant
#define DW_AT_decl_file 0x3a             // class constant
#define DW_AT_decl_line 0x3b             // class constant
#define DW_AT_declaration 0x3c           // class flag
#define DW_AT_discr_list 0x3d            // class block
#define DW_AT_encoding 0x3e              // class constant
#define DW_AT_external 0x3f              // class flag
#define DW_AT_frame_base 0x40            // class block, loclistptr
#define DW_AT_friend 0x41                // class reference
#define DW_AT_identifier_case 0x42       // class constant
#define DW_AT_macro_info 0x43            // class macptr - up to version 4
#define DW_AT_namelist_item 0x44         // class block
#define DW_AT_priority 0x45              // class reference
#define DW_AT_segment 0x46               // class block, loclistptr
#define DW_AT_specification 0x47         // class reference
#define DW_AT_static_link 0x48           // class block, loclistptr
#define DW_AT_type 0x49                  // class reference
#define DW_AT_use_location 0x4a          // class block, loclistptr
#define DW_AT_variable_parameter 0x4b    // class flag
#define DW_AT_virtuality 0x4c            // class constant
#define DW_AT_vtable_elem_location 0x4d  // class block, loclistptr
#define DW_AT_allocated 0x4e             // class block, constant, reference
#define DW_AT_associated 0x4f            // class block, constant, reference
#define DW_AT_data_location 0x50         // class block
#define DW_AT_byte_stride 0x51           // class block, constant, reference
#define DW_AT_entry_pc 0x52              // class address
#define DW_AT_use_UTF8 0x53              // class flag
#define DW_AT_extension 0x54             // class reference
#define DW_AT_ranges 0x55                // class rangelistptr
#define DW_AT_trampoline 0x56            // class address, flag, reference, string
#define DW_AT_call_column 0x57           // class constant
#define DW_AT_call_file 0x58             // class constant
#define DW_AT_call_line 0x59             // class constant
#define DW_AT_description 0x5a           // class string
#define DW_AT_binary_scale 0x5b          // class constant
#define DW_AT_decimal_scale 0x5c         // class constant
#define DW_AT_small 0x5d                 // class reference
#define DW_AT_decimal_sign 0x5e          // class constant
#define DW_AT_digit_count 0x5f           // class constant
#define DW_AT_picture_string 0x60        // class string
#define DW_AT_mutable 0x61               // class flag
#define DW_AT_threads_scaled 0x62        // class flag
#define DW_AT_explicit 0x63              // class flag
#define DW_AT_object_pointer 0x64        // class reference
#define DW_AT_endianity 0x65             // class constant
#define DW_AT_elemental 0x66             // class flag
#define DW_AT_pure 0x67                  // class flag
#define DW_AT_recursive 0x68             // class flag

// version 5
#define DW_AT_signature 0x69                // class reference
#define DW_AT_main_subprogram 0x6a          // class flag
#define DW_AT_data_bit_offset 0x6b          // class constant
#define DW_AT_const_expr 0x6c               // class flag
#define DW_AT_enum_class 0x6d               // class flag
#define DW_AT_linkage_name 0x6e             // class string
#define DW_AT_string_length_bit_size 0x6f   // class constant
#define DW_AT_string_length_byte_size 0x70  // class constant
#define DW_AT_rank 0x71                     // class constant, exprloc
#define DW_AT_str_offsets_base 0x72         // class stroffsetsptr
#define DW_AT_addr_base 0x73                // class addrptr
#define DW_AT_rnglists_base 0x74            // class rnglistsptr
#define DW_AT_dwo_name 0x76                 // class string
#define DW_AT_reference 0x77                // class flag
#define DW_AT_rvalue_reference 0x78         // class flag
#define DW_AT_macros 0x79                   // class macptr
#define DW_AT_call_all_calls 0x7a           // class flag
#define DW_AT_call_all_source_calls 0x7b    // class flag
#define DW_AT_call_all_tail_calls 0x7c      // class flag
#define DW_AT_call_return_pc 0x7d           // class address
#define DW_AT_call_value 0x7e               // class exprloc
#define DW_AT_call_origin 0x7f              // class exprloc
#define DW_AT_call_parameter 0x80           // class reference
#define DW_AT_call_pc 0x81                  // class address
#define DW_AT_call_tail_call 0x82           // class flag
#define DW_AT_call_target 0x83              // class exprloc
#define DW_AT_call_target_clobbered 0x84    // class exprloc
#define DW_AT_call_data_location 0x85       // class exprloc
#define DW_AT_call_data_value 0x86          // class exprloc
#define DW_AT_noreturn 0x87                 // class flag
#define DW_AT_alignment 0x88                // class constant
#define DW_AT_export_symbols 0x89           // class flag
#define DW_AT_deleted 0x8a                  // class flag
#define DW_AT_defaulted 0x8b                // class constant
#define DW_AT_loclists_base 0x8c            // class loclistsptr

// normal attributes
#define DW_AT_lo_user 0x2000  // class ---
#define DW_AT_hi_user 0x3fff  // class ---

// attribute forms
#define DW_FORM_addr 0x01        // class address
#define DW_FORM_block2 0x03      // class block
#define DW_FORM_block4 0x04      // class block
#define DW_FORM_data2 0x05       // class constant
#define DW_FORM_data4 0x06       // class constant, lineptr, loclistptr, macptr, rangelistptr
#define DW_FORM_data8 0x07       // class constant, lineptr, loclistptr, macptr, rangelistptr
#define DW_FORM_string 0x08      // class string
#define DW_FORM_block 0x09       // class block
#define DW_FORM_block1 0x0a      // class block
#define DW_FORM_data1 0x0b       // class constant
#define DW_FORM_flag 0x0c        // class flag
#define DW_FORM_sdata 0x0d       // class constant
#define DW_FORM_strp 0x0e        // class string
#define DW_FORM_udata 0x0f       // class constant
#define DW_FORM_ref_addr 0x10    // class reference
#define DW_FORM_ref1 0x11        // class reference
#define DW_FORM_ref2 0x12        // class reference
#define DW_FORM_ref4 0x13        // class reference
#define DW_FORM_ref8 0x14        // class reference
#define DW_FORM_ref_udata 0x15   // class reference
#define DW_FORM_indirect 0x16    // class (see Section 7.5.3)
#define DW_FORM_sec_offset 0x17  // class addrptr, lineptr, loclist, loclistsptr,
// macptr, rnglist, rnglistsptr, stroffsetsptr
#define DW_FORM_exprloc 0x18         // class exprloc
#define DW_FORM_flag_present 0x19    // class flag
#define DW_FORM_strx 0x1a            // class string
#define DW_FORM_addrx 0x1b           // class address
#define DW_FORM_ref_sup4 0x1c        // class reference
#define DW_FORM_strp_sup 0x1d        // class string
#define DW_FORM_data16 0x1e          // class constant
#define DW_FORM_line_strp 0x1f       // class string
#define DW_FORM_ref_sig8 0x20        // class reference
#define DW_FORM_implicit_const 0x21  // class constant
#define DW_FORM_loclistx 0x22        // class loclist
#define DW_FORM_rnglistx 0x23        // class rnglist
#define DW_FORM_ref_sup8 0x24        // class reference
#define DW_FORM_strx1 0x25           // class string
#define DW_FORM_strx2 0x26           // class string
#define DW_FORM_strx3 0x27           // class string
#define DW_FORM_strx4 0x28           // class string
#define DW_FORM_addrx1 0x29          // class address
#define DW_FORM_addrx2 0x2a          // class address
#define DW_FORM_addrx3 0x2b          // class address
#define DW_FORM_addrx4 0x2c          // class address

// unit types
#define DW_UT_compile 0x01
#define DW_UT_type 0x02
#define DW_UT_partial 0x03
#define DW_UT_skeleton 0x04
#define DW_UT_split_compile 0x05
#define DW_UT_split_type 0x06
#define DW_UT_lo_user 0x80
#define DW_UT_hi_user 0xff

// range lists
#define DW_RLE_end_of_list 0x00
#define DW_RLE_base_addressx 0x01
#define DW_RLE_startx_endx 0x02
#define DW_RLE_startx_length 0x03
#define DW_RLE_offset_pair 0x04
#define DW_RLE_base_address 0x05
#define DW_RLE_start_end 0x06
#define DW_RLE_start_length 0x07

// line program
#define DW_LNS_copy 0x01
#define DW_LNS_advance_pc 0x02
#define DW_LNS_advance_line 0x03
#define DW_LNS_set_file 0x04
#define DW_LNS_set_column 0x05
#define DW_LNS_negate_stmt 0x06
#define DW_LNS_set_basic_block 0x07
#define DW_LNS_const_add_pc 0x08
#define DW_LNS_fixed_advance_pc 0x09
#define DW_LNS_set_prologue_end 0x0a
#define DW_LNS_set_epilogue_begin 0x0b
#define DW_LNS_set_isa 0x0c
#define DW_LNE_end_sequence 0x01
#define DW_LNE_set_address 0x02
#define DW_LNE_set_discriminator 0x04
#define DW_LNE_lo_user 0x80
#define DW_LNE_hi_user 0xf
#define DW_LNCT_path 0x1
#define DW_LNCT_directory_index 0x2
#define DW_LNCT_timestamp 0x3
#define DW_LNCT_size 0x4
#define DW_LNCT_MD5 0x5
#define DW_LNCT_lo_user 0x2000
#define DW_LNCT_hi_user 0x3fff

extern const char* getDwarfTagName(int tagName);
extern const char* getDwarfAttributeName(int attName);
extern const char* getDwarfFormName(int formName);

// class interpretation

// address: relocated address of size according to addressSize in compilation unit header (when
// using DW_FORM_addr). When using DW_FORM_addrx (value is LEB128), or DW_FORM_addrx1, 2, 3, 4
// (value has fixed size), this is an indirect index into a table of addresses in the .debug_addr
// section. The index is relative to the value of the DW_AT_addr_base attribute of the associated
// compilation unit.

// addrptr: This is an offset into the .debug_addr section (DW_FORM_sec_offset). consists of an
// offset from the beginning of the .debug_addr section to the beginning of the list of machine
// addresses information for the referencing entity.

// block: block size (according to DW_FORM_block1, 2, 4), or unsigned LEB128 (with DW_FORM_block),
// followed by block data according to block size

// constant: constant value, either sized (DW_FORM_data1, 2, 4, 8, 16) or LEB128 (with DW_FORM_sdata
// and DW_FORM_udata).

// exprloc: unsigned LEB128 length followed by the number of information bytes specified by the
// length (DW_FORM_exprloc). The information bytes contain a DWARF expression or location
// description.

// flag: explict single byte (zero/non-zero) with DW_FORM_flag, or implicitly with
// DW_FORM_flag_present. In the latter case there is no value, as it is already implied as 1.

// lineptr: offset to .debug_line section. either 4 or 8 byte (DW_FORM_data4, 8)

// loclist: An index into the .debug_loclists section (DW_FORM_loclistx), or An offset into the
// .debug_loclists section (DW_FORM_sec_offset).

// loclistptr: offset into .debug_loc section. either 4 or 8 byte (DW_FORM_data4, 8)

// macptr: offset into .debug_macinfo section. either 4 or 8 byte (DW_FORM_data4, 8)

// rangelist: An index into the .debug_rnglists section (DW_FORM_rnglistx), or An offset into the
// .debug_rnglists section (DW_FORM_sec_offset).

// rangelistptr: offset into .debug_ranges section. either 4 or 8 byte (DW_FORM_data4, 8)

// reference: identify data within a compilation unit. for internal reference this is an offset from
// the first byte of the compilation header for the compilation unit containing the reference. Could
// be fixed length offset (with DW_FORM_ref1, 2, 4, 8), or variable (DW_FORM_ref_udata).
// for external reference, DW_FORM_ref_addr is used,and it is an offset from the beginning of
// the .debug_info section of the target executable or shared object.
// a third type exists with DW_FORM_ref_sig8 - is the 8-byte type signature identify any debugging
// information type entry that has been placed in its own type unit
// the fourth type is with DW_FORM_ref_sup4, 8 - is a reference from within the .debug_info section
// of the executable or shared object file to a debugging information entry in the .debug_info
// section of a supplementary object file.

// string: a sequence of contiguous non-null bytes followed by one null byte. When used with
// DW_FORM_string, the value is specified within the CU, otherwise with DW_FORM_strp, it specifies
// an offset into the .debug_str section. If CU header has address size of 32, then 4 bytes are used
// for the offset, otherwise 8 bytes are used.
// third form - DW_FORM_line_strp - an offset into a string table contained in the .debug_line_str
// section of the object file.
// fourth form - DW_FORM_strp_sup - an offset into a string table contained in the .debug_str
// section of a supplementary object file
// fifth form - an indirect offset into the string table using an index into a table of offsets
// contained in the .debug_str_offsets section of the object file. Used with DW_FORM_strx (LEB128),
// DW_FORM_strx1, 2, 3, 4 (fixed size value).

// stroffsetptr: This is an offset into the .debug_str_offsets section (DW_FORM_sec_offset). It
// consists of an offset from the beginning of the .debug_str_offsets section to the beginning of
// the string offsets information for the referencing entity.

#endif  // __DWARF_DEF_H__