/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2007 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.0 of the PHP license,       |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_0.txt.                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Masaki Fujimoto <fujimoto@php.net>                           |
  +----------------------------------------------------------------------+
*/
#include "php_fuse.h"

#if HAVE_FUSE

#define MAKE_LONG_ZVAL(name, val)	MAKE_STD_ZVAL(name); ZVAL_LONG(name, val);

/* {{{ fuse_functions[] */
zend_function_entry fuse_functions[] = {
	{ NULL, NULL, NULL }
};
/* }}} */

ZEND_DECLARE_MODULE_GLOBALS(fuse)

/* {{{ fuse_module_entry
 */
zend_module_entry fuse_module_entry = {
	STANDARD_MODULE_HEADER,
	"fuse",
	fuse_functions,
	PHP_MINIT(fuse),
	PHP_MSHUTDOWN(fuse), 
	PHP_RINIT(fuse),
	NULL,
	PHP_MINFO(fuse),
	"0.9.2", 
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_FUSE
ZEND_GET_MODULE(fuse)
#endif

/* 
 in addition, you will want to define all the methods required by fuse operations here
*/
#define PHP_FUSE_VALIDATE_METHOD "validate"

/* {{{ */
static zend_class_entry *php_fuse_ce;
static zend_object_handlers php_fuse_object_handlers;
static zend_object_handlers php_fuse_object_handlers_ze1;
/* }}} */

/* {{{ fuse object handlers */
static void php_fuse_object_free_storage(void *object TSRMLS_DC) {
	php_fuse_object *intern = (php_fuse_object*)object;
	
	zend_object_std_dtor(&intern->zo TSRMLS_CC);
	efree(object);
}

static zend_object_value php_fuse_object_handler_new(zend_class_entry *ce TSRMLS_DC) {
	zend_object_value retval;
	php_fuse_object *intern;
	
	intern = emalloc(sizeof(php_fuse_object));
	memset(intern, 0, sizeof(php_fuse_object));

	zend_object_std_init(&intern->zo, ce TSRMLS_CC);
#if PHP_VERSION_ID < 50399
	zend_hash_copy(intern->zo.properties, &ce->default_properties, (copy_ctor_func_t)zval_add_ref, (void*)&tmp, sizeof(zval*));
#else
	object_properties_init((zend_object*) intern, ce);
#endif
	retval.handle = zend_objects_store_put(intern, (zend_objects_store_dtor_t)zend_objects_destroy_object, (zend_objects_free_object_storage_t)php_fuse_object_free_storage, NULL TSRMLS_CC);
#if PHP_VERSION_ID < 50300
	retval.handlers = EG(ze1_compatibility_mode) ? &php_fuse_object_handlers_ze1 : &php_fuse_object_handlers;
#else
	retval.handlers = &php_fuse_object_handlers;
#endif

	return retval;
}
/* }}} */

/*
 This is used as the callback function to call a callback function.
	The userland implementation of Fuse::validate will be called on each argument from command line
	If ::validate() returns false from userland, parsing is halted and FAILURE is returned
	If ::validate() returns true from userland, the argument is added to pfo->fargs and SUCCESS is returned
*/
int php_fuse_parse_opt_proc(zval **pzthis, const char *arg, int key, struct fuse_args *args) {
	zval                    *retval, *param;
	php_fuse_object			*pfo = (php_fuse_object *) zend_object_store_get_object(*pzthis TSRMLS_CC);
	int                     result = SUCCESS;
	
	if (pfo) {
		ALLOC_INIT_ZVAL(param);

		Z_TYPE_P(param) = IS_STRING;		
		Z_STRVAL_P(param) = estrndup(arg, strlen(arg));
		Z_STRLEN_P(param) = strlen(arg);
		Z_SET_REFCOUNT_P(param, 1);
		
		zend_call_method_with_1_params(pzthis, pfo->zo.ce, NULL, PHP_FUSE_VALIDATE_METHOD, &retval, param);

		if (retval) {
			convert_to_boolean(retval);
			
			if (Z_BVAL_P(retval)) {
				fuse_opt_add_arg(&pfo->fargs, arg);
			} else result = FAILURE;		
			
			zval_ptr_dtor(&retval);
		}
		
		zval_ptr_dtor(&param);			
	}
	
	return result;
}

static PHP_METHOD(Fuse, __construct) {
	zval *argv;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &argv) == FAILURE) {
		return;
	}

	if(!zend_hash_num_elements(Z_ARRVAL_P(argv))) {
		php_error(E_NOTICE,"no arguments passed");
		RETURN_FALSE;
	}

	if (getThis()) {
		struct fuse_args fargs = {0, NULL, 0};
		{
			/* populate input args from console */
			zval **pzval;
			
			for(zend_hash_internal_pointer_reset(Z_ARRVAL_P(argv)); 
				zend_hash_get_current_data(Z_ARRVAL_P(argv), (void**) &pzval) == SUCCESS; 
				zend_hash_move_forward(Z_ARRVAL_P(argv))) {
				if (Z_TYPE_PP(pzval) != IS_STRING) {
					convert_to_string_ex(pzval);
				}
				
				fuse_opt_add_arg(&fargs, Z_STRVAL_PP(pzval));
			}
		}
		
		{
			/* parse arguments using callback */
			switch (fuse_opt_parse(&fargs, (void *)&getThis(), NULL, (fuse_opt_proc_t) php_fuse_parse_opt_proc)) {
				case SUCCESS:
					/* if you are here, then the argument have passed validation and are available at pfo->fargs */
					RETURN_TRUE;
				break;

				default: {
					/* if you are here, then the arguments have failed validation */
				}
			}
		}
		
		/* this is the temporary structure used while executing validate() */
		fuse_opt_free_args(&fargs);
	}

	RETURN_FALSE;
}

/* }}} */

/* {{{ fuse method entries */
ZEND_BEGIN_ARG_INFO_EX(php_fuse_constructor_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, argv)				// argv[]
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_INFO_EX(php_fuse_validate_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, arg)				// string arg
ZEND_END_ARG_INFO()

zend_function_entry php_fuse_methods[] = {
	PHP_ME            (Fuse,    __construct,    php_fuse_constructor_arginfo,  ZEND_ACC_PUBLIC|ZEND_ACC_FINAL)
	PHP_ABSTRACT_ME   (Fuse,    validate,       php_fuse_validate_arginfo)
	{NULL, NULL, NULL}
};
/* }}} */

static void php_fuse_globals_ctor(zend_fuse_globals *globals TSRMLS_DC) {
	pthread_mutex_init(&globals->mutex, NULL);
}

static void php_fuse_globals_dtor(zend_fuse_globals *globals TSRMLS_DC) {
	pthread_mutex_destroy(&globals->mutex);
}

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(fuse) {
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "Fuse", php_fuse_methods); 
	php_fuse_ce = zend_register_internal_class(&ce TSRMLS_CC);
	php_fuse_ce->create_object = php_fuse_object_handler_new;

	memcpy(&php_fuse_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	memcpy(&php_fuse_object_handlers_ze1, zend_get_std_object_handlers(), sizeof(zend_object_handlers));

	/* constants */
	REGISTER_LONG_CONSTANT("FUSE_S_IFMT", S_IFMT, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_S_IFSOCK", S_IFSOCK, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_S_IFLNK", S_IFLNK, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_S_IFREG", S_IFREG, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_S_IFBLK", S_IFBLK, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_S_IFDIR", S_IFDIR, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_S_IFCHR", S_IFCHR, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_S_IFIFO", S_IFIFO, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_S_ISUID", S_ISUID, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_S_ISGID", S_ISGID, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_S_ISVTX", S_ISVTX, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_S_IRWXU", S_IRWXU, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_S_IRUSR", S_IRUSR, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_S_IWUSR", S_IWUSR, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_S_IXUSR", S_IXUSR, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_S_IRWXG", S_IRWXG, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_S_IRGRP", S_IRGRP, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_S_IWGRP", S_IWGRP, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_S_IXGRP", S_IXGRP, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_S_IRWXO", S_IRWXO, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_S_IROTH", S_IROTH, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_S_IWOTH", S_IWOTH, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_S_IXOTH", S_IXOTH, CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("FUSE_EPERM", EPERM, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_ENOENT", ENOENT, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_ESRCH", ESRCH, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_EINTR", EINTR, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_EIO", EIO, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_ENXIO", ENXIO, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_E2BIG", E2BIG, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_ENOEXEC", ENOEXEC, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_EBADF", EBADF, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_ECHILD", ECHILD, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_EAGAIN", EAGAIN, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_ENOMEM", ENOMEM, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_EACCES", EACCES, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_EFAULT", EFAULT, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_ENOTBLK", ENOTBLK, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_EBUSY", EBUSY, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_EEXIST", EEXIST, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_EXDEV", EXDEV, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_ENODEV", ENODEV, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_ENOTDIR", ENOTDIR, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_EISDIR", EISDIR, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_EINVAL", EINVAL, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_ENFILE", ENFILE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_EMFILE", EMFILE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_ENOTTY", ENOTTY, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_ETXTBSY", ETXTBSY, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_EFBIG", EFBIG, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_ENOSPC", ENOSPC, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_ESPIPE", ESPIPE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_EROFS", EROFS, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_EMLINK", EMLINK, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_EPIPE", EPIPE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_EDOM", EDOM, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_ERANGE", ERANGE, CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("FUSE_DT_UKNOWN", DT_UNKNOWN, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_DT_REG", DT_REG, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_DT_DIR", DT_DIR, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_DT_FIFO", DT_FIFO, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_DT_SOCK", DT_SOCK, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_DT_CHR", DT_CHR, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_DT_BLK", DT_BLK, CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("FUSE_O_RDONLY", O_RDONLY, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_O_WRONLY", O_WRONLY, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_O_RDWR", O_RDWR, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_O_CREAT", O_CREAT, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_O_EXCL", O_EXCL, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_O_NOCTTY", O_NOCTTY, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_O_TRUNC", O_TRUNC, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_O_APPEND", O_APPEND, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_O_NONBLOCK", O_NONBLOCK, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_O_NDELAY", O_NDELAY, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_O_SYNC", O_SYNC, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_O_FSYNC", O_FSYNC, CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("FUSE_XATTR_CREATE", XATTR_CREATE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FUSE_XATTR_REPLACE", XATTR_REPLACE, CONST_CS | CONST_PERSISTENT);
	
	REGISTER_LONG_CONSTANT("FUSE_OPT_KEY_NONOPT", FUSE_OPT_KEY_NONOPT, CONST_CS | CONST_PERSISTENT);
	
	ZEND_INIT_MODULE_GLOBALS(fuse, php_fuse_globals_ctor, php_fuse_globals_dtor);
	
	return SUCCESS;
}
/* }}} */

PHP_MSHUTDOWN_FUNCTION(fuse) 
{
#ifndef ZTS
	php_fuse_globals_dtor(&fuse_globals TSRMLS_CC);
#endif
}

/* {{{ PHP_RINIT_FUNCTION */
PHP_RINIT_FUNCTION(fuse) {
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(fuse) {
	php_info_print_box_start(0);
	php_printf("<p>FUSE(File system in USEr space) Bindings for PHP</p>\n");
	php_printf("<p>Version 0.9.2 (2009-10-15)</p>\n");
	php_info_print_box_end();
}
/* }}} */

#endif /* HAVE_FUSE */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
