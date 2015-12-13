/*
  Copyright (C) 2015-2015 David Anderson. All Rights Reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of version 2.1 of the GNU Lesser General Public License
  as published by the Free Software Foundation.

  This program is distributed in the hope that it would be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  Further, this software is distributed without any warranty that it is
  free of the rightful claim of any third person regarding infringement
  or the like.  Any license provided herein, whether implied or
  otherwise, applies only to this software file.  Patent licenses, if
  any, provided herein do not apply to combinations of this program with
  other software, or any other product whatsoever.

  You should have received a copy of the GNU Lesser General Public
  License along with this program; if not, write the Free Software
  Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston MA 02110-1301,
  USA.

*/

#include "config.h"
#include "dwarf_incl.h"
#include <stdio.h>
#include <limits.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */
#include "dwarf_macro5.h"

# if 0
#define LEFTPAREN '('
#define RIGHTPAREN ')'
#define SPACE ' '
#endif
#define TRUE 1
#define FALSE 0

/*  Section 6.3: Macro Information:
    Each macro unit ends with an entry
    containing an opcode of 0. */

static const Dwarf_Small dwarf_udata_string_form[]  = {DW_FORM_udata,DW_FORM_string};
static const Dwarf_Small dwarf_udata_udata_form[]   = {DW_FORM_udata,DW_FORM_udata};
static const Dwarf_Small dwarf_udata_strp_form[]    = {DW_FORM_udata,DW_FORM_strp};
static const Dwarf_Small dwarf_udata_strp_sup_form[] = {DW_FORM_udata,DW_FORM_strp_sup};
static const Dwarf_Small dwarf_secoffset_form[]     = {DW_FORM_sec_offset};
static const Dwarf_Small dwarf_udata_strx_form[]    = {DW_FORM_udata,DW_FORM_strx};

#if 0
/* We do not presently use this here. It's a view of DW2 macros. */
struct Dwarf_Macro_Forms_s dw2formsarray[] = {
    {0,0,0},
    {DW_MACINFO_define,2,dwarf_udata_string_form},
    {DW_MACINFO_undef,2,dwarf_udata_string_form},
    {DW_MACINFO_start_file,2,dwarf_udata_udata_form},
    {DW_MACINFO_end_file,0,0},
    {DW_MACINFO_vendor_ext,2,dwarf_udata_string_form},
};

/* Represents original DWARF 2,3,4 macro info */
static const struct Dwarf_Macro_OperationsList_s dwarf_default_macinfo_opslist= {
6, dw2formsarray
};
#endif

struct Dwarf_Macro_Forms_s dw5formsarray[] = {
    {0,0,0},
    {DW_MACRO_define,2,dwarf_udata_string_form},
    {DW_MACRO_undef,2,dwarf_udata_string_form},
    {DW_MACRO_start_file,2,dwarf_udata_udata_form},
    {DW_MACRO_end_file,0,0},

    {DW_MACRO_define_strp,2,dwarf_udata_strp_form},
    {DW_MACRO_undef_strp,2,dwarf_udata_strp_form},
    {DW_MACRO_import,1,dwarf_secoffset_form},

    {DW_MACRO_define_sup,2,dwarf_udata_strp_sup_form},
    {DW_MACRO_undef_sup,2,dwarf_udata_strp_sup_form},
    {DW_MACRO_import_sup,1,dwarf_secoffset_form},

    {DW_MACRO_define_strx,2,dwarf_udata_strx_form},
    {DW_MACRO_undef_strx,2,dwarf_udata_strx_form},
};



/* Represents DWARF 5 macro info */
/* .debug_macro predefined, in order by value  */
static const struct Dwarf_Macro_OperationsList_s dwarf_default_macro_opslist = {
13, dw5formsarray
};

int _dwarf_internal_macro_context_by_offset(Dwarf_Debug dbg,
    Dwarf_Unsigned offset,
    Dwarf_Unsigned  * version_out,
    Dwarf_Macro_Context * macro_context_out,
    Dwarf_Error * error);
int _dwarf_internal_macro_context(Dwarf_Die die,
    Dwarf_Unsigned  * version_out,
    Dwarf_Macro_Context * macro_context_out,
    Dwarf_Error * error);



static int
is_std_moperator(Dwarf_Small op)
{
    if (op > 1 && op <= DW_MACRO_undef_strx) {
        return TRUE;
    }
    return FALSE;
}

static int
_dwarf_skim_forms(Dwarf_Debug dbg,
  Dwarf_Macro_Context mcontext,
  Dwarf_Small *mdata_start,
  unsigned formcount,
  const Dwarf_Small *forms,
  Dwarf_Small *section_end,
  Dwarf_Unsigned *forms_length,
    Dwarf_Error *error)
{
    unsigned i = 0;
    Dwarf_Small curform = 0 ;
    Dwarf_Unsigned totallen = 0;
    Dwarf_Unsigned v = 0;
    Dwarf_Unsigned ret_value = 0;
    Dwarf_Unsigned length;
    Dwarf_Small *mdata = mdata_start;
    Dwarf_Word leb128_length = 0;

    for( ; i < formcount; ++i) {
        curform = forms[i];
        if (mdata >= section_end) {
            _dwarf_error(dbg, error, DW_DLE_MACRO_OFFSET_BAD);
            return DW_DLV_ERROR;
        }
        switch(curform) {
        default:
            _dwarf_error(dbg,error,DW_DLE_DEBUG_FORM_HANDLING_INCOMPLETE);
            return DW_DLV_ERROR;
        case DW_FORM_block1:
            v =  *(Dwarf_Small *) mdata;
            totallen += v+1;
            mdata += v+1;
            break;
        case DW_FORM_block2:
            READ_UNALIGNED(dbg, ret_value, Dwarf_Unsigned,
                mdata, sizeof(Dwarf_Half));
            v = ret_value + sizeof(Dwarf_Half);
            totallen += v;
            mdata += v;
            break;
        case DW_FORM_block4:
            READ_UNALIGNED(dbg, ret_value, Dwarf_Unsigned,
                mdata, sizeof(Dwarf_ufixed));
            v = ret_value + sizeof(Dwarf_ufixed);
            totallen += v;
            mdata += v;
            break;
        case DW_FORM_data1:
            v = 1;
            totallen += v;
            mdata += v;
            break;
        case DW_FORM_data2:
            v = 2;
            totallen += v;
            mdata += v;
            break;
        case DW_FORM_data4:
            v = 4;
            totallen += v;
            mdata += v;
            break;
        case DW_FORM_data8:
            v = 8;
            totallen += v;
            mdata += v;
            break;
        case DW_FORM_data16:
            v = 8;
            totallen += v;
            mdata += v;
            break;
        case DW_FORM_string:
            v = strlen((char *) mdata) + 1;
            totallen += v;
            mdata += v;
            break;
        case DW_FORM_block:
            length = _dwarf_decode_u_leb128(mdata, &leb128_length);
            v = length + leb128_length;
            totallen += v;
            mdata += v;
            break;
        case DW_FORM_flag:
            v = 1;
            totallen += v;
            mdata += v;
            break;
        case DW_FORM_sec_offset:
            /* If 32bit dwarf, is 4. Else is 64bit dwarf and is 8. */
            v = mcontext->mc_offset_size;
            totallen += v;
            mdata += v;
            break;
        case DW_FORM_sdata:
            /*  Discard the decoded value, we just want the length
                of the value. */
            _dwarf_decode_s_leb128(mdata, &leb128_length);
            v = leb128_length;
            mdata += v;
            totallen += v;
            break;
        case DW_FORM_strx:
            _dwarf_decode_u_leb128(mdata, &leb128_length);
            v = leb128_length;
            mdata += v;
            totallen += v;
            break;
        case DW_FORM_strp:
            v = mcontext->mc_offset_size;
            mdata += v;
            totallen += v;
            break;
        case DW_FORM_udata:
            /*  Discard the decoded value, we just want the length
                of the value. */
            _dwarf_decode_u_leb128(mdata, &leb128_length);
            v = leb128_length;
            mdata += v;
            totallen += v;
            break;
        }
    }
    if (mdata > section_end) {
        _dwarf_error(dbg, error, DW_DLE_MACRO_OFFSET_BAD);
        return DW_DLV_ERROR;
    }
    *forms_length = totallen;
    return DW_DLV_OK;
}

int
dwarf_get_macro_ops_count(Dwarf_Macro_Context macro_context,
    Dwarf_Unsigned * ops_count_out,
    Dwarf_Unsigned * ops_total_length,
    Dwarf_Error *error)
{
    Dwarf_Debug dbg = 0;
    Dwarf_Small *mdata = 0;
    Dwarf_Small *section_end = 0;
    Dwarf_Small *section_base = 0;
    Dwarf_Unsigned opcount = 0;
    int res = 0;

    dbg = macro_context->mc_dbg;
    section_base = dbg->de_debug_macro.dss_data;
    section_end = section_base + dbg->de_debug_macro.dss_size;
    mdata = macro_context->mc_macro_ops;
    while (mdata < section_end) {
        Dwarf_Small op = 0;

        if (mdata >= section_end)  {
            _dwarf_error(dbg, error, DW_DLE_MACRO_PAST_END);
            return DW_DLV_ERROR;
        }
        op = *mdata;
        ++opcount;
        ++mdata;
        if (!op) {
            Dwarf_Unsigned opslen = 0;
            /*  End of ops, this is terminator, count the ending 0
                as an operator so dwarfdump can print it.  */
            opslen = mdata - macro_context->mc_macro_ops;
            *ops_count_out = opcount;
            *ops_total_length = opslen;
            macro_context->mc_total_length = opslen +
                macro_context->mc_macro_header_length;
            return DW_DLV_OK;
        }
        if (is_std_moperator(op)) {
            struct Dwarf_Macro_Forms_s * ourform =
                dw5formsarray + op;
            /* ASSERT: op == ourform->mf_code */
            unsigned formcount = ourform->mf_formcount;
            const Dwarf_Small *forms = ourform->mf_formbytes;
            Dwarf_Unsigned forms_length = 0;

            res = _dwarf_skim_forms(dbg,macro_context,mdata,
                formcount,forms,
                section_end,
                &forms_length,error);
            if ( res != DW_DLV_OK) {
                return res;
            }
            mdata += forms_length;
        } else {
            /* FIXME Add support for user defined ops. */
            _dwarf_error(dbg, error, DW_DLE_MACRO_OP_UNHANDLED);
            return DW_DLV_ERROR;
        }
        if (mdata > section_end)  {
            _dwarf_error(dbg, error, DW_DLE_MACRO_PAST_END);
            return DW_DLV_ERROR;
        }
    }
    _dwarf_error(dbg, error, DW_DLE_MACRO_PAST_END);
    return DW_DLV_ERROR;
}

/* op_number: 0 to ops_count_out -1. */
int
dwarf_get_macro_op(Dwarf_Macro_Context macro_context,
    Dwarf_Unsigned op_number,
    Dwarf_Half    * op_start_section_offset,
    Dwarf_Half    * macro_operator,
    Dwarf_Half    * forms_count,
    const char **   formcode_array,
    Dwarf_Error *error)
{
    /* FIXME */
    _dwarf_error(macro_context->mc_dbg, error, DW_DLE_MACRO_OP_UNHANDLED);
    return DW_DLV_ERROR;
}

int
dwarf_get_macro_defundef(Dwarf_Macro_Context macro_context,
    Dwarf_Unsigned op_number,
    Dwarf_Unsigned * line_number,
    Dwarf_Unsigned * index_or_offset,
    Dwarf_Unsigned * index_offset,
    Dwarf_Half    * forms_count,
    const char    ** macro_string,
    Dwarf_Error *error)
{
    /* FIXME */
    _dwarf_error(macro_context->mc_dbg, error, DW_DLE_MACRO_OP_UNHANDLED);
    return DW_DLV_ERROR;
}

int
dwarf_get_macro_startend_file(Dwarf_Macro_Context macro_context,
    Dwarf_Unsigned op_number,
    Dwarf_Unsigned * line_number,
    Dwarf_Unsigned * name_index_to_line_tab,
    const char    ** src_file_name,
    Dwarf_Error *error)
{
    /* FIXME */
    _dwarf_error(macro_context->mc_dbg, error, DW_DLE_MACRO_OP_UNHANDLED);
    return DW_DLV_ERROR;
}

/*  Target_offset is the offset in a .debug_macro section,
    of a macro unit header. */
int
dwarf_get_macro_import(Dwarf_Macro_Context macro_context,
    Dwarf_Unsigned op_number,
    Dwarf_Unsigned * target_offset,
    Dwarf_Error *error)
{
    /* FIXME */
    _dwarf_error(macro_context->mc_dbg, error, DW_DLE_MACRO_OP_UNHANDLED);
    return DW_DLV_ERROR;
}


static int
valid_form_in_list(Dwarf_Half form)
{
    switch(form) {
    case DW_FORM_block:
    case DW_FORM_block1:
    case DW_FORM_block2:
    case DW_FORM_block4:
    case DW_FORM_data1:
    case DW_FORM_data2:
    case DW_FORM_data4:
    case DW_FORM_data8:
    case DW_FORM_data16:
    case DW_FORM_sdata:
    case DW_FORM_udata:
    case DW_FORM_flag:
    case DW_FORM_sec_offset:
    case DW_FORM_string:
    case DW_FORM_strp:
    case DW_FORM_strx:
        return TRUE;
    }
    return FALSE;
}

static int
validate_opcode(Dwarf_Debug dbg,
   struct Dwarf_Macro_Forms_s *curform,
   Dwarf_Error * error)
{
    unsigned i = 0;
    struct Dwarf_Macro_Forms_s *stdfptr = 0;
    if (curform->mf_code >= DW_MACRO_lo_user) {
        /* Nothing to check. user level. */
        return DW_DLV_OK;
    }
    if (curform->mf_code > DW_MACRO_undef_strx) {
        _dwarf_error(dbg, error, DW_DLE_MACRO_OPCODE_BAD);
        return (DW_DLV_ERROR);
    }
    if (!curform->mf_code){
        _dwarf_error(dbg, error, DW_DLE_MACRO_OPCODE_BAD);
        return (DW_DLV_ERROR);
    }
    stdfptr = &dwarf_default_macro_opslist.mol_data[curform->mf_code];

    if (curform->mf_formcount != stdfptr->mf_formcount) {
        _dwarf_error(dbg, error, DW_DLE_MACRO_OPCODE_FORM_BAD);
        return (DW_DLV_ERROR);
    }
    for(i = 0; i < curform->mf_formcount; ++i) {
        if (curform->mf_formbytes[i] != stdfptr->mf_formbytes[1]) {
            _dwarf_error(dbg, error, DW_DLE_MACRO_OPCODE_FORM_BAD);
            return (DW_DLV_ERROR);
        }
    }
    return DW_DLV_OK;
}

static int
read_operands_table(Dwarf_Macro_Context macro_context,
    Dwarf_Small * macro_header,
    Dwarf_Small * macro_data,
    Dwarf_Small * section_base,
    Dwarf_Unsigned section_size,
    Dwarf_Unsigned *table_size_out,
    Dwarf_Error *error)
{
    Dwarf_Small* table_data_start = macro_data;
    Dwarf_Unsigned local_size = 0;
    Dwarf_Unsigned cur_offset = 0;
    Dwarf_Small operand_table_count = 0;
    unsigned i = 0;
    struct Dwarf_Macro_Forms_s *curformentry = 0;
    Dwarf_Debug dbg = 0;

    dbg = macro_context->mc_dbg;
    cur_offset = (1+ macro_data) - macro_header;
    if (cur_offset >= section_size) {
        _dwarf_error(dbg, error, DW_DLE_MACRO_OFFSET_BAD);
        return (DW_DLV_ERROR);
    }
    READ_UNALIGNED(dbg,operand_table_count,Dwarf_Small,
        macro_data,sizeof(Dwarf_Small));
    macro_data += sizeof(Dwarf_Small);
    /* Estimating minimum size */
    local_size = operand_table_count * 4;

    cur_offset = (local_size+ macro_data) - section_base;
    if (cur_offset >= section_size) {
        _dwarf_error(dbg, error, DW_DLE_MACRO_OFFSET_BAD);
        return (DW_DLV_ERROR);
    }
    /* first, get size of table. */
    table_data_start = macro_data;
    for (i = 0; i < operand_table_count; ++i) {
        /*  Compiler warning about unused opcode_number
            variable should be ignored. */
        Dwarf_Small opcode_number = 0;
        Dwarf_Unsigned formcount = 0;
        Dwarf_Word uleblen = 0;
        READ_UNALIGNED(dbg,opcode_number,Dwarf_Small,
            macro_data,sizeof(Dwarf_Small));
        macro_data += sizeof(Dwarf_Small);

        formcount = _dwarf_decode_u_leb128(macro_data,
            &uleblen);
        macro_data += uleblen;
        cur_offset = (formcount+ macro_data) - section_base;
        if (cur_offset >= section_size) {
            _dwarf_error(dbg, error, DW_DLE_MACRO_OFFSET_BAD);
            return (DW_DLV_ERROR);
        }
        macro_data += formcount;
    }
    /* reset for reread. */
    macro_data = table_data_start;
    /* allocate table */
    macro_context->mc_opcode_forms =  (struct Dwarf_Macro_Forms_s *)
        calloc(operand_table_count,
            sizeof(struct Dwarf_Macro_Forms_s));
    macro_context->mc_opcode_count = operand_table_count;
    if(!macro_context->mc_opcode_forms) {
        _dwarf_error(dbg, error, DW_DLE_ALLOC_FAIL);
        return DW_DLV_ERROR;
    }

    curformentry = macro_context->mc_opcode_forms;
    for (i = 0; i < operand_table_count; ++i,++curformentry) {
        Dwarf_Small opcode_number = 0;
        Dwarf_Unsigned formcount = 0;
        Dwarf_Word uleblen = 0;

        cur_offset = (2 + macro_data) - section_base;
        if (cur_offset >= section_size) {
            _dwarf_error(dbg, error, DW_DLE_MACRO_OFFSET_BAD);
            return (DW_DLV_ERROR);
        }
        READ_UNALIGNED(dbg,opcode_number,Dwarf_Small,
            macro_data,sizeof(Dwarf_Small));
        macro_data += sizeof(Dwarf_Small);
        formcount = _dwarf_decode_u_leb128(macro_data,
            &uleblen);
        curformentry->mf_code = opcode_number;
        curformentry->mf_formcount = formcount;
        macro_data += uleblen;
        cur_offset = (formcount+ macro_data) - section_base;
        if (cur_offset >= section_size) {
            _dwarf_error(dbg, error, DW_DLE_MACRO_OFFSET_BAD);
            return (DW_DLV_ERROR);
        }
        curformentry->mf_formbytes = macro_data;
        macro_data += formcount;
        if (opcode_number  > DW_MACRO_undef_strx ) {
            Dwarf_Half k = 0;
            for(k = 0; k < formcount; ++k) {
                if (!valid_form_in_list(curformentry->mf_formbytes[k])) {
                    _dwarf_error(dbg, error, DW_DLE_MACRO_OP_UNHANDLED);
                    return (DW_DLV_ERROR);
                }
            }
        }
        int res = validate_opcode(macro_context->mc_dbg,curformentry, error);
        if(res != DW_DLV_OK) {
            return res;
        }
    }
    *table_size_out = macro_data - table_data_start;
    return DW_DLV_OK;
}

int
_dwarf_internal_macro_context(Dwarf_Die die,
    Dwarf_Unsigned  * version_out,
    Dwarf_Macro_Context * macro_context_out,
    Dwarf_Error * error)
{
    Dwarf_CU_Context   cu_context = 0;

    /*  The Dwarf_Debug this die belongs to. */
    Dwarf_Debug dbg = 0;
    int resattr = DW_DLV_ERROR;
    int lres = DW_DLV_ERROR;
    int res = DW_DLV_ERROR;
    Dwarf_Unsigned macro_offset = 0;
    Dwarf_Attribute macro_attr = 0;

    /*  ***** BEGIN CODE ***** */
    if (error != NULL) {
        *error = NULL;
    }

    CHECK_DIE(die, DW_DLV_ERROR);
    cu_context = die->di_cu_context;
    dbg = cu_context->cc_dbg;

    /*  Doing the load here results in duplication of the
        section-load call  (in the by_offset
        interface below) but detects the missing section
        quickly. */
    res = _dwarf_load_section(dbg, &dbg->de_debug_macro,error);
    if (res != DW_DLV_OK) {
        return res;
    }
    if (!dbg->de_debug_macro.dss_size) {
        return (DW_DLV_NO_ENTRY);
    }

    resattr = dwarf_attr(die, DW_AT_macros, &macro_attr, error);
    if (resattr == DW_DLV_NO_ENTRY) {
        resattr = dwarf_attr(die, DW_AT_GNU_macros, &macro_attr, error);
    }
    if (resattr != DW_DLV_OK) {
        return resattr;
    }
    lres = dwarf_global_formref(macro_attr, &macro_offset, error);
    if (lres != DW_DLV_OK) {
        return lres;
    }
    lres = _dwarf_internal_macro_context_by_offset(dbg,
        macro_offset,version_out,macro_context_out,error);
    return lres;
}

int
_dwarf_internal_macro_context_by_offset(Dwarf_Debug dbg,
    Dwarf_Unsigned offset,
    Dwarf_Unsigned  * version_out,
    Dwarf_Macro_Context * macro_context_out,
    Dwarf_Error * error)
{
    Dwarf_Unsigned line_table_offset = 0;
    Dwarf_Small * macro_header = 0;
    Dwarf_Small * macro_data = 0;
    Dwarf_Half version = 0;
    Dwarf_Small flags = 0;
    Dwarf_Small offset_size = 4;
    Dwarf_Unsigned cur_offset = 0;
    Dwarf_Unsigned section_size = 0;
    Dwarf_Small *section_base = 0;
    Dwarf_Unsigned optablesize = 0;
    Dwarf_Unsigned macro_offset = offset;
    int res = 0;
    Dwarf_Macro_Context macro_context = 0;

    res = _dwarf_load_section(dbg, &dbg->de_debug_macro,error);
    if (res != DW_DLV_OK) {
        return res;
    }
    if (!dbg->de_debug_macro.dss_size) {
        return (DW_DLV_NO_ENTRY);
    }

    section_base = dbg->de_debug_macro.dss_data;
    section_size = dbg->de_debug_macro.dss_size;
    /*  The '3'  ensures the header initial bytes present too. */
    if ((3+macro_offset) >= section_size) {
        _dwarf_error(dbg, error, DW_DLE_MACRO_OFFSET_BAD);
        return (DW_DLV_ERROR);
    }
    macro_header = macro_offset + section_base;
    macro_data = macro_header;


    macro_context = (Dwarf_Macro_Context)
        _dwarf_get_alloc(dbg,DW_DLA_MACRO_CONTEXT,1);
    if (!macro_context) {
        _dwarf_error(dbg, error, DW_DLE_ALLOC_FAIL);
        return DW_DLV_ERROR;
    }

    READ_UNALIGNED(dbg,version, Dwarf_Half,
        macro_data,sizeof(Dwarf_Half));
    macro_data += sizeof(Dwarf_Half);
    READ_UNALIGNED(dbg,flags, Dwarf_Small,
        macro_data,sizeof(Dwarf_Small));
    macro_data += sizeof(Dwarf_Small);

    macro_context->mc_macro_header = macro_header;
    macro_context->mc_section_offset = macro_offset;
    macro_context->mc_version_number = version;
    macro_context->mc_flags = flags;
    macro_context->mc_dbg = dbg;
    macro_context->mc_offset_size_flag =
        flags& MACRO_OFFSET_SIZE_FLAG?TRUE:FALSE;
    macro_context->mc_debug_line_offset_flag =
        flags& MACRO_LINE_OFFSET_FLAG?TRUE:FALSE;
    macro_context->mc_operands_table_flag =
        flags& MACRO_OP_TABLE_FLAG?TRUE:FALSE;
    offset_size = macro_context->mc_offset_size_flag?8:4;
    macro_context->mc_offset_size = offset_size;
    if (macro_context->mc_debug_line_offset_flag) {
        cur_offset = (offset_size+ macro_data) - section_base;
        if (cur_offset >= section_size) {
            dwarf_dealloc_macro_context(macro_context);
            _dwarf_error(dbg, error, DW_DLE_MACRO_OFFSET_BAD);
            return (DW_DLV_ERROR);
        }
        READ_UNALIGNED(dbg,line_table_offset,Dwarf_Unsigned,
            macro_data,offset_size);
        macro_data += offset_size;
        macro_context->mc_debug_line_offset = line_table_offset;
    }
    if (macro_context->mc_operands_table_flag) {
        res = read_operands_table(macro_context,
            macro_header,
            macro_data,
            section_base,
            section_size,
            &optablesize,
            error);
        if (res != DW_DLV_OK) {
            dwarf_dealloc_macro_context(macro_context);
            return res;
        }
    }
    macro_data += optablesize;
    macro_context->mc_macro_ops = macro_data;
    macro_context->mc_macro_header_length =macro_data - macro_header;
    *version_out = version;
    *macro_context_out = macro_context;
    return DW_DLV_OK;
}

int dwarf_macro_context_head(Dwarf_Macro_Context head,
    Dwarf_Half     * version,
    Dwarf_Unsigned * mac_offset,
    Dwarf_Unsigned * mac_len,
    Dwarf_Unsigned * mac_header_len,
    unsigned *       flags,
    Dwarf_Bool *     has_line_offset,
    Dwarf_Bool *     has_offset_size_64,
    Dwarf_Bool *     has_operands_table,
    Dwarf_Half *     opcode_count,
    Dwarf_Error *error)
{
    if (!head || head->mc_sentinel != 0xada) {
        Dwarf_Debug dbg = 0;
        if(head) {
            dbg = head->mc_dbg;
        }
        _dwarf_error(dbg, error,DW_DLE_BAD_MACRO_HEADER_POINTER);
        return DW_DLV_ERROR;
    }
    *version = head->mc_version_number;
    *mac_offset = head->mc_section_offset;
    *mac_len    = head->mc_total_length;
    *mac_header_len = head->mc_macro_header_length;
    *flags = head->mc_flags;
    *has_line_offset = head->mc_debug_line_offset_flag;
    *has_offset_size_64 = head->mc_offset_size_flag;
    *has_operands_table = head->mc_operands_table_flag;
    *opcode_count = head->mc_opcode_count;
    return DW_DLV_OK;
}
int dwarf_macro_operands_table(Dwarf_Macro_Context head,
    Dwarf_Half  index, /* 0 to opcode_count -1 */
    Dwarf_Half  *opcode_number,
    Dwarf_Half  *operand_count,
    const Dwarf_Small **operand_array,
    Dwarf_Error *error)
{
    struct Dwarf_Macro_Forms_s * ops = 0;
    Dwarf_Debug dbg = 0;
    if (!head || head->mc_sentinel != 0xada) {
        if(head) {
            dbg = head->mc_dbg;
        }
        _dwarf_error(dbg, error,DW_DLE_BAD_MACRO_HEADER_POINTER);
        return DW_DLV_ERROR;
    }
    dbg = head->mc_dbg;
    if (index < 0 || index >= head->mc_opcode_count) {
        _dwarf_error(dbg, error, DW_DLE_BAD_MACRO_INDEX);
        return DW_DLV_ERROR;
    }
    ops = head->mc_opcode_forms + index;
    *opcode_number = ops->mf_code;
    *operand_count = ops->mf_formcount;
    *operand_array = ops->mf_formbytes;
    return DW_DLV_OK;
}

/*  The base interface to the .debug_macro section data
    for a specific CU.

    The version number passed back by *version_out
    may be 4 (a gnu extension of DWARF)  or 5. */
int
dwarf_get_macro_context(Dwarf_Die cu_die,
    Dwarf_Unsigned  * version_out,
    Dwarf_Macro_Context * macro_context,
    Dwarf_Error * error)
{
    int res = 0;

    res =  _dwarf_internal_macro_context(cu_die,version_out,
        macro_context, error);
    return res;
}
int
dwarf_get_macro_context_by_offset(Dwarf_Debug dbg,
    Dwarf_Unsigned offset,
    Dwarf_Unsigned  * version_out,
    Dwarf_Macro_Context * macro_context,
    Dwarf_Error * error)
{
    int res = 0;

    res =  _dwarf_internal_macro_context_by_offset(dbg,
        offset,version_out,
        macro_context, error);
    return res;
}

int dwarf_get_macro_section_name(Dwarf_Debug dbg,
   const char **sec_name_out,
   Dwarf_Error *error)
{
    struct Dwarf_Section_s *sec = 0;

    sec = &dbg->de_debug_macro;
    if (sec->dss_size == 0) {
        /* We don't have such a  section at all. */
        return DW_DLV_NO_ENTRY;
    }
    *sec_name_out = sec->dss_name;
    return DW_DLV_OK;
}

void
dwarf_dealloc_macro_context(Dwarf_Macro_Context mc)
{
    Dwarf_Debug dbg = mc->mc_dbg;
    dwarf_dealloc(dbg,mc,DW_DLA_MACRO_CONTEXT);
}

int
_dwarf_macro_constructor(Dwarf_Debug dbg, void *m)
{
    /* Nothing to do */
    Dwarf_Macro_Context mc= (Dwarf_Macro_Context)m;
    /* Arbitrary sentinel. For debugging. */
    mc->mc_sentinel = 0xada;
    return DW_DLV_OK;
}
void
_dwarf_macro_destructor(void *m)
{
    Dwarf_Macro_Context mc= (Dwarf_Macro_Context)m;
    if (mc->mc_opcode_forms) {
        free(mc->mc_opcode_forms);
        mc->mc_opcode_forms = 0;
        memset(m,0,sizeof(*mc));
    }
    /* Just a recognizable sentinel. For debugging.  No real meaning. */
    mc->mc_sentinel = 0xdeadbeef;
}