/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2012 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Felipe Pena <felipe@php.net>                                 |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_udis86.h"

/* True global resources - no need for thread safety here */
static int le_udis86;

#ifdef COMPILE_DL_UDIS86
ZEND_GET_MODULE(udis86)
#endif

static void udis86_resource_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC) /* {{{ */
{
	if (rsrc->ptr) {
		php_udis86_obj *ud = rsrc->ptr;
		if (ud->stream) {
			php_stream_close(ud->stream);
			ud->stream = NULL;
		}
		efree(rsrc->ptr);
		rsrc->ptr = NULL;
	}
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(udis86)
{
	/* For udis86_set_syntax(): */
	REGISTER_LONG_CONSTANT("UDIS86_ATT",   PHP_UDIS86_ATT,   CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("UDIS86_INTEL", PHP_UDIS86_INTEL, CONST_CS|CONST_PERSISTENT);	
	/* For udis86_set_vendor(): */
	REGISTER_LONG_CONSTANT("UDIS86_AMD",   PHP_UDIS86_AMD,   CONST_CS|CONST_PERSISTENT);
	
	le_udis86 = zend_register_list_destructors_ex(
		udis86_resource_dtor, NULL, "udis86", module_number);	
	
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(udis86)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(udis86)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "udis86 support", "enabled");
	php_info_print_table_end();
}
/* }}} */

/* {{{ proto resource udis86_init(void)
   Return a resource of an initialized udis86 data */
static PHP_FUNCTION(udis86_init)
{
	php_udis86_obj *ud_obj;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}
	
	ud_obj = emalloc(sizeof(php_udis86_obj));
	
	ud_init(&ud_obj->ud);
	ud_set_syntax(&ud_obj->ud, UD_SYN_ATT);
	ud_set_input_buffer(&ud_obj->ud, "", 0);
	
	ud_obj->stream = NULL;
	
	ZEND_REGISTER_RESOURCE(return_value, ud_obj, le_udis86);
}
/* }}} */

/* {{{ proto void udis86_close(resource obj)
   Close the current working stream being used */
static PHP_FUNCTION(udis86_close)
{
	php_udis86_obj *ud_obj;
	zval *ud;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r",
		&ud) == FAILURE) {
		return;
	}
	
	ZEND_FETCH_RESOURCE(ud_obj, php_udis86_obj*, &ud, -1, "udis86", le_udis86);
	
	if (ud_obj->stream) {
		php_stream_close(ud_obj->stream);
		ud_obj->stream = NULL;
	}
}

/* {{{ proto void udis86_set_mode(int mode)
   Set the mode of disassembly */
static PHP_FUNCTION(udis86_set_mode) 
{
	php_udis86_obj *ud_obj;
	zval *ud;
	long mode;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rl",
		&ud, &mode) == FAILURE) {
		return;
	}
	
	ZEND_FETCH_RESOURCE(ud_obj, php_udis86_obj*, &ud, -1, "udis86", le_udis86);
	
	switch (mode) {
		case 16:
		case 32:
		case 64:
			ud_set_mode(&ud_obj->ud, mode);
			break;
		default:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "invalid mode");
	}
}
/* }}} */

/* {{{ proto bool udis86_input_file(resource obj, string file)
   Set the stream as internal buffer, return false if any error ocurred,
   otherwise true */
static PHP_FUNCTION(udis86_input_file) 
{
	php_udis86_obj *ud_obj;
	zval *ud;
	char *fname;
	int fname_len;
	FILE *fp = NULL;
	php_stream *stream;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs",
		&ud, &fname, &fname_len) == FAILURE) {
		return;
	}
	
	ZEND_FETCH_RESOURCE(ud_obj, php_udis86_obj*, &ud, -1, "udis86", le_udis86);
	
	if (ud_obj->stream != NULL) {
		php_stream_close(ud_obj->stream);
		ud_obj->stream = NULL;
	}

	stream = php_stream_open_wrapper(fname, "rb", REPORT_ERRORS, NULL);
	
	if (stream == NULL) {
		RETURN_FALSE;
	}
		
	if (php_stream_cast(stream, PHP_STREAM_AS_STDIO, (void**)&fp, 
		REPORT_ERRORS) == FAILURE) {
		php_stream_close(stream);
		RETURN_FALSE;
	}
	
	ud_obj->stream = stream;
	
	ud_set_input_file(&ud_obj->ud, fp);
	
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto int udis86_disassemble(void)
   Disassemble the internal buffer and return the number of bytes
   disassembled */
static PHP_FUNCTION(udis86_disassemble)
{
	php_udis86_obj *ud_obj;
	zval *ud;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r",
		&ud) == FAILURE) {
		return;
	}
	
	ZEND_FETCH_RESOURCE(ud_obj, php_udis86_obj*, &ud, -1, "udis86", le_udis86);
	
	if (ud_obj->stream == NULL) {
		RETURN_LONG(0);
	}
	
	RETURN_LONG(ud_disassemble(&ud_obj->ud));
}
/* }}} */

/* {{{ proto string udis86_insn_asm(resource obj)
   Return the string representation of disassembled instruction */
static PHP_FUNCTION(udis86_insn_asm)
{
	php_udis86_obj *ud_obj;
	zval *ud;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r",
		&ud) == FAILURE) {
		return;
	}
	
	ZEND_FETCH_RESOURCE(ud_obj, php_udis86_obj*, &ud, -1, "udis86", le_udis86);
	
	RETURN_STRING(ud_insn_asm(&ud_obj->ud), 1);
}
/* }}} */

/* {{{ proto int udis86_insn_len(resource obj)
   Return the number of bytes of the disassembled instruction */
static PHP_FUNCTION(udis86_insn_len)
{
	php_udis86_obj *ud_obj;
	zval *ud;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r",
		&ud) == FAILURE) {
		return;
	}
	
	ZEND_FETCH_RESOURCE(ud_obj, php_udis86_obj*, &ud, -1, "udis86", le_udis86);
	
	RETURN_LONG(ud_insn_len(&ud_obj->ud));
}
/* }}} */

/* {{{ proto string udis86_insn_hex(resource obj)
   Return the hexadecimal representation of the disassembled instruction */
static PHP_FUNCTION(udis86_insn_hex)
{
	php_udis86_obj *ud_obj;
	zval *ud;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r",
		&ud) == FAILURE) {
		return;
	}
	
	ZEND_FETCH_RESOURCE(ud_obj, php_udis86_obj*, &ud, -1, "udis86", le_udis86);
	
	RETURN_STRING(ud_insn_hex(&ud_obj->ud), 1);
}
/* }}} */

/* {{{ proto int udis86_insn_off(resource obj)
   Return the offset of the disassembled instruction relative to program
   counter value specified internally */
static PHP_FUNCTION(udis86_insn_off)
{
	php_udis86_obj *ud_obj;
	zval *ud;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r",
		&ud) == FAILURE) {
		return;
	}
	
	ZEND_FETCH_RESOURCE(ud_obj, php_udis86_obj*, &ud, -1, "udis86", le_udis86);
	
	RETURN_LONG(ud_insn_off(&ud_obj->ud));
}
/* }}} */

/* {{{ proto void udis86_input_skip(resource obj, int n)
   Skips N bytes in the input stream */
static PHP_FUNCTION(udis86_input_skip)
{
	php_udis86_obj *ud_obj;
	zval *ud;
	long n;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rl",
		&ud, &n) == FAILURE) {
		return;
	}
	
	ZEND_FETCH_RESOURCE(ud_obj, php_udis86_obj*, &ud, -1, "udis86", le_udis86);
	
	ud_input_skip(&ud_obj->ud, n);
}
/* }}} */

/* {{{ proto void udis86_set_pc(resource obj, int pc)
   Set the program counter */
static PHP_FUNCTION(udis86_set_pc)
{
	php_udis86_obj *ud_obj;
	zval *ud;
	long pc;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rl",
		&ud, &pc) == FAILURE) {
		return;
	}
	
	ZEND_FETCH_RESOURCE(ud_obj, php_udis86_obj*, &ud, -1, "udis86", le_udis86);
	
	ud_set_pc(&ud_obj->ud, pc);
}
/* }}} */

/* {{{ proto void udis86_set_syntax(resource obj, int syntax)
   Set the syntax to be used in the disassembly representation */
static PHP_FUNCTION(udis86_set_syntax)
{
	php_udis86_obj *ud_obj;
	zval *ud;
	long syntax;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rl",
		&ud, &syntax) == FAILURE) {
		return;
	}
	
	ZEND_FETCH_RESOURCE(ud_obj, php_udis86_obj*, &ud, -1, "udis86", le_udis86);
	
	switch (syntax) {
		case PHP_UDIS86_ATT:
			ud_set_syntax(&ud_obj->ud, UD_SYN_ATT);
			break;
		case PHP_UDIS86_INTEL:
			ud_set_syntax(&ud_obj->ud, UD_SYN_INTEL);
			break;
		default:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "invalid syntax");
	}
}
/* }}} */

/* {{{ proto void udis86_set_vendor(resource obj, int vendor)
   Set the vendor of whose instruction to choose from */
static PHP_FUNCTION(udis86_set_vendor)
{
	php_udis86_obj *ud_obj;
	zval *ud;
	long vendor;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rl",
		&ud, &vendor) == FAILURE) {
		return;
	}
	
	ZEND_FETCH_RESOURCE(ud_obj, php_udis86_obj*, &ud, -1, "udis86", le_udis86);
	
	switch (vendor) {
		case PHP_UDIS86_AMD:
			ud_set_vendor(&ud_obj->ud, UD_VENDOR_AMD);
			break;
		case PHP_UDIS86_INTEL:
			ud_set_vendor(&ud_obj->ud, UD_VENDOR_INTEL);
			break;
		default:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "invalid vendor");
	}
}
/* }}} */

/* {{{ arginfo 
 */
ZEND_BEGIN_ARG_INFO_EX(arginfo_udis86_void, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_udis86_obj_only, 0, 0, 1)
	ZEND_ARG_INFO(0, obj)
ZEND_END_ARG_INFO()	

ZEND_BEGIN_ARG_INFO_EX(arginfo_udis86_input_file, 0, 0, 2)
	ZEND_ARG_INFO(0, obj)
	ZEND_ARG_INFO(0, file)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_udis86_input_skip, 0, 0, 2)
	ZEND_ARG_INFO(0, obj)
	ZEND_ARG_INFO(0, mode)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_udis86_set_mode, 0, 0, 2)
	ZEND_ARG_INFO(0, obj)
	ZEND_ARG_INFO(0, num)
ZEND_END_ARG_INFO()
/* }}} */

/* {{{ udis86_functions[]
 */
const zend_function_entry udis86_functions[] = {
	PHP_FE(udis86_init,        arginfo_udis86_void)
	PHP_FE(udis86_close,       arginfo_udis86_obj_only)
	PHP_FE(udis86_input_file,  arginfo_udis86_input_file)
	PHP_FE(udis86_disassemble, arginfo_udis86_obj_only)
	PHP_FE(udis86_insn_asm,    arginfo_udis86_obj_only)
	PHP_FE(udis86_insn_len,    arginfo_udis86_obj_only)
	PHP_FE(udis86_insn_hex,    arginfo_udis86_obj_only)
	PHP_FE(udis86_insn_off,    arginfo_udis86_obj_only)
	PHP_FE(udis86_input_skip,  arginfo_udis86_input_skip)
	PHP_FE(udis86_set_mode,    arginfo_udis86_set_mode)
	PHP_FE(udis86_set_pc,      arginfo_udis86_set_mode)
	PHP_FE(udis86_set_syntax,  arginfo_udis86_set_mode)
	PHP_FE(udis86_set_vendor,  arginfo_udis86_set_mode)
	PHP_FE_END
};
/* }}} */

/* {{{ udis86_module_entry
 */
zend_module_entry udis86_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"udis86",
	udis86_functions,
	PHP_MINIT(udis86),
	PHP_MSHUTDOWN(udis86),
	NULL,
	NULL,
	PHP_MINFO(udis86),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_UDIS86_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
