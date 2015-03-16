#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include "test.h"
#include "../src/alloc.h"
#include "../src/conf.h"
#include "../src/conffile.h"

static struct conf **setup(void)
{
	struct conf **confs;
	alloc_counters_reset();
	confs=confs_alloc();
	confs_init(confs);
	return confs;
}

static void tear_down(struct conf ***confs)
{
	confs_free(confs);
	fail_unless(free_count, alloc_count);
}

struct data
{
        const char *str;
	const char *field;
	const char *value;
};

static struct data d[] = {
	{ "a=b", "a", "b" },
	{ "a=b\n", "a", "b" },
	{ "a = b", "a", "b" },
	{ "   a  =    b ", "a", "b" },
	{ "   a  =    b \n", "a", "b" },
	{ "#a=b", NULL, NULL },
	{ "  #a=b", NULL, NULL },
	{ "a='b'", "a", "b" },
	{ "a='b", "a", "b" },
	{ "a=b'", "a", "b'" },
	{ "a=\"b\"", "a", "b" },
	{ "a=b\"", "a", "b\"" },
	{ "a=\"b", "a", "b" },
	{ "a=b # comment", "a", "b # comment" }, // Maybe fix this.
	{ "field=longvalue with spaces", "field", "longvalue with spaces" },
};

START_TEST(test_conf_get_pair)
{
        FOREACH(d)
	{
		char *field=NULL;
		char *value=NULL;
		char *str=strdup(d[i].str);
		conf_get_pair(str, &field, &value);
		if(!field || !d[i].field)
			fail_unless(field==d[i].field);
		else
			fail_unless(!strcmp(field, d[i].field));
		if(!value || !d[i].value)
			fail_unless(value==d[i].value);
		else
			fail_unless(!strcmp(value, d[i].value));
		free(str);
	}
}
END_TEST

#define MIN_CLIENT_CONF				\
	"mode=client\n"				\
	"server=4.5.6.7\n"			\
	"port=1234\n"				\
	"status_port=12345\n"			\
	"lockfile=/lockfile/path\n"		\
	"ssl_cert=/ssl/cert/path\n"		\
	"ssl_cert_ca=/cert_ca/path\n"		\
	"ssl_peer_cn=my_cn\n"			\
	"ca_csr_dir=/csr/dir\n"			\
	"ssl_key=/ssl/key/path\n"		\

START_TEST(test_client_conf)
{
	struct conf **confs=setup();
	fail_unless(!conf_load_global_only_buf(MIN_CLIENT_CONF, confs));
	fail_unless(get_e_burp_mode(confs[OPT_BURP_MODE])==BURP_MODE_CLIENT);
	ck_assert_str_eq(get_string(confs[OPT_SERVER]), "4.5.6.7");
	ck_assert_str_eq(get_string(confs[OPT_PORT]), "1234");
	ck_assert_str_eq(get_string(confs[OPT_STATUS_PORT]), "12345");
	ck_assert_str_eq(get_string(confs[OPT_LOCKFILE]), "/lockfile/path");
	ck_assert_str_eq(get_string(confs[OPT_SSL_CERT]), "/ssl/cert/path");
	ck_assert_str_eq(get_string(confs[OPT_SSL_CERT_CA]), "/cert_ca/path");
	ck_assert_str_eq(get_string(confs[OPT_SSL_PEER_CN]), "my_cn");
	ck_assert_str_eq(get_string(confs[OPT_SSL_KEY]), "/ssl/key/path");
	ck_assert_str_eq(get_string(confs[OPT_CA_CSR_DIR]), "/csr/dir");
	tear_down(&confs);
}
END_TEST

static void assert_strlist(struct strlist **s, const char *path, int flag)
{
	if(!path)
	{
		fail_unless(*s==NULL);
		return;
	}
	ck_assert_str_eq((*s)->path, path);
	fail_unless((*s)->flag==flag);
	*s=(*s)->next;
}

static void assert_include(struct strlist **s, const char *path)
{
	assert_strlist(s, path, 1);
}

static void assert_exclude(struct strlist **s, const char *path)
{
	assert_strlist(s, path, 0);
}

START_TEST(test_client_includes_excludes)
{
	const char *buf=MIN_CLIENT_CONF
		"exclude=/z\n"
		"exclude=/a/b\n"
		"include=/a/b/c\n"
		"include=/x/y/z\n"
		"include=/r/s/t\n"
		"include=/a\n"
	;
	struct strlist *s;
	struct conf **confs;
	confs=setup();
	fail_unless(!conf_load_global_only_buf(buf, confs));
	s=get_strlist(confs[OPT_INCLUDE]);
	assert_include(&s, "/a");
	assert_include(&s, "/a/b/c");
	assert_include(&s, "/r/s/t");
	assert_include(&s, "/x/y/z");
	assert_include(&s, NULL);
	s=get_strlist(confs[OPT_EXCLUDE]);
	assert_exclude(&s, "/a/b");
	assert_exclude(&s, "/z");
	assert_exclude(&s, NULL);
	s=get_strlist(confs[OPT_STARTDIR]);
	assert_include(&s, "/a");
	assert_include(&s, "/r/s/t");
	assert_include(&s, "/x/y/z");
	assert_include(&s, NULL);
	s=get_strlist(confs[OPT_INCEXCDIR]);
	assert_include(&s, "/a");
	assert_exclude(&s, "/a/b");
	assert_include(&s, "/a/b/c");
	assert_include(&s, "/r/s/t");
	assert_include(&s, "/x/y/z");
	assert_exclude(&s, "/z");
	assert_include(&s, NULL);
	tear_down(&confs);
}
END_TEST

static const char *include_failures[] = {
	MIN_CLIENT_CONF "include=not_absolute\n",
	MIN_CLIENT_CONF "include=/\ninclude=/\n"
};

START_TEST(test_client_include_failures)
{
	struct conf **confs;
	FOREACH(include_failures)
	{
		confs=setup();
		fail_unless(conf_load_global_only_buf(include_failures[i],
			confs)==-1);
		tear_down(&confs);
	}
}
END_TEST

#define MIN_SERVER_CONF				\
	"mode=server\n"				\
	"port=1234\n"				\
	"status_port=12345\n"			\
	"lockfile=/lockfile/path\n"		\
	"ssl_cert=/ssl/cert/path\n"		\
	"ssl_cert_ca=/cert_ca/path\n"		\
	"directory=/a/directory\n"		\
	"dedup_group=a_group\n" 		\
	"clientconfdir=/a/ccdir\n"		\
	"ssl_dhfile=/a/dhfile\n"		\
	"keep=10\n"				\

START_TEST(test_server_conf)
{
	struct strlist *s;
	struct conf **confs=setup();
	fail_unless(!conf_load_global_only_buf(MIN_SERVER_CONF, confs));
	fail_unless(get_e_burp_mode(confs[OPT_BURP_MODE])==BURP_MODE_SERVER);
	ck_assert_str_eq(get_string(confs[OPT_PORT]), "1234");
	ck_assert_str_eq(get_string(confs[OPT_STATUS_PORT]), "12345");
	ck_assert_str_eq(get_string(confs[OPT_LOCKFILE]), "/lockfile/path");
	ck_assert_str_eq(get_string(confs[OPT_SSL_CERT]), "/ssl/cert/path");
	ck_assert_str_eq(get_string(confs[OPT_SSL_CERT_CA]), "/cert_ca/path");
	ck_assert_str_eq(get_string(confs[OPT_DIRECTORY]), "/a/directory");
	ck_assert_str_eq(get_string(confs[OPT_DEDUP_GROUP]), "a_group");
	ck_assert_str_eq(get_string(confs[OPT_CLIENTCONFDIR]), "/a/ccdir");
	ck_assert_str_eq(get_string(confs[OPT_SSL_DHFILE]), "/a/dhfile");
	s=get_strlist(confs[OPT_KEEP]);
	assert_strlist(&s, "10", 10);
	assert_include(&s, NULL);
	tear_down(&confs);
}
END_TEST

static void pre_post_checks(const char *buf, const char *pre_path,
	const char *post_path, const char *pre_arg1, const char *pre_arg2,
	const char *post_arg1, const char *post_arg2,
	enum conf_opt o_script_pre, enum conf_opt o_script_post,
	enum conf_opt o_script_pre_arg, enum conf_opt o_script_post_arg,
	enum conf_opt o_script_pre_notify, enum conf_opt o_script_post_notify,
	enum conf_opt o_script_post_run_on_fail)
{
	struct strlist *s;
	struct conf **confs;
	confs=setup();
	fail_unless(!conf_load_global_only_buf(buf, confs));
	ck_assert_str_eq(get_string(confs[o_script_pre]), pre_path);
	ck_assert_str_eq(get_string(confs[o_script_post]), post_path);
	s=get_strlist(confs[o_script_pre_arg]);
	assert_strlist(&s, pre_arg1, 0);
	assert_strlist(&s, pre_arg2, 0);
	assert_strlist(&s, NULL, 0);
	if(o_script_pre_notify!=OPT_MAX)
		fail_unless(get_int(confs[o_script_pre_notify])==1);
	s=get_strlist(confs[o_script_post_arg]);
	assert_strlist(&s, post_arg1, 0);
	assert_strlist(&s, post_arg2, 0);
	assert_strlist(&s, NULL, 0);
	if(o_script_post_notify!=OPT_MAX)
		fail_unless(get_int(confs[o_script_post_notify])==1);
	fail_unless(get_int(confs[o_script_post_run_on_fail])==1);
	tear_down(&confs);
}

static void server_pre_post_checks(const char *buf, const char *pre_path,
	const char *post_path, const char *pre_arg1, const char *pre_arg2,
	const char *post_arg1, const char *post_arg2)
{
	pre_post_checks(buf, pre_path, post_path, pre_arg1, pre_arg2,
		post_arg1, post_arg2,
		OPT_S_SCRIPT_PRE, OPT_S_SCRIPT_POST,
		OPT_S_SCRIPT_PRE_ARG, OPT_S_SCRIPT_POST_ARG,
		OPT_S_SCRIPT_PRE_NOTIFY, OPT_S_SCRIPT_POST_NOTIFY,
		OPT_S_SCRIPT_POST_RUN_ON_FAIL);
}

START_TEST(test_server_script_pre_post)
{
	const char *buf=MIN_SERVER_CONF
		"server_script_pre=pre_path\n"
		"server_script_pre_arg=pre_arg1\n"
		"server_script_pre_arg=pre_arg2\n"
		"server_script_pre_notify=1\n"
		"server_script_post=post_path\n"
		"server_script_post_arg=post_arg1\n"
		"server_script_post_arg=post_arg2\n"
		"server_script_post_notify=1\n"
		"server_script_post_run_on_fail=1\n"
	;
	server_pre_post_checks(buf, "pre_path", "post_path", "pre_arg1",
		"pre_arg2", "post_arg1", "post_arg2");
}
END_TEST

// Same as test_server_script_pre_post, but use server_script to set both pre
// and post at the same time.
START_TEST(test_server_script)
{
	const char *buf=MIN_SERVER_CONF
		"server_script=path\n"
		"server_script_arg=arg1\n"
		"server_script_arg=arg2\n"
		"server_script_notify=1\n"
		"server_script_notify=1\n"
		"server_script_run_on_fail=1\n"
		"server_script_post_run_on_fail=1\n"
	;
	server_pre_post_checks(buf, "path", "path", "arg1",
		"arg2", "arg1", "arg2");
}
END_TEST

static void backup_script_pre_post_checks(const char *buf, const char *pre_path,
	const char *post_path, const char *pre_arg1, const char *pre_arg2,
	const char *post_arg1, const char *post_arg2)
{
	pre_post_checks(buf, pre_path, post_path, pre_arg1, pre_arg2,
		post_arg1, post_arg2,
		OPT_B_SCRIPT_PRE, OPT_B_SCRIPT_POST,
		OPT_B_SCRIPT_PRE_ARG, OPT_B_SCRIPT_POST_ARG,
		OPT_MAX, OPT_MAX, OPT_B_SCRIPT_POST_RUN_ON_FAIL);
}

START_TEST(test_backup_script_pre_post)
{
	const char *buf=MIN_CLIENT_CONF
		"backup_script_pre=pre_path\n"
		"backup_script_pre_arg=pre_arg1\n"
		"backup_script_pre_arg=pre_arg2\n"
		"backup_script_post=post_path\n"
		"backup_script_post_arg=post_arg1\n"
		"backup_script_post_arg=post_arg2\n"
		"backup_script_post_run_on_fail=1\n"
	;
	backup_script_pre_post_checks(buf, "pre_path", "post_path", "pre_arg1",
		"pre_arg2", "post_arg1", "post_arg2");
}
END_TEST

// Same as test_backup_script_pre_post, but use backup_script to set both pre
// and post at the same time.
START_TEST(test_backup_script)
{
	const char *buf=MIN_CLIENT_CONF
		"backup_script=path\n"
		"backup_script_arg=arg1\n"
		"backup_script_arg=arg2\n"
		"backup_script_run_on_fail=1\n"
		"backup_script_post_run_on_fail=1\n"
	;
	backup_script_pre_post_checks(buf, "path", "path", "arg1",
		"arg2", "arg1", "arg2");
}
END_TEST

static void restore_script_pre_post_checks(const char *buf,
	const char *pre_path, const char *post_path,
	const char *pre_arg1, const char *pre_arg2,
	const char *post_arg1, const char *post_arg2)
{
	pre_post_checks(buf, pre_path, post_path, pre_arg1, pre_arg2,
		post_arg1, post_arg2,
		OPT_R_SCRIPT_PRE, OPT_R_SCRIPT_POST,
		OPT_R_SCRIPT_PRE_ARG, OPT_R_SCRIPT_POST_ARG,
		OPT_MAX, OPT_MAX, OPT_R_SCRIPT_POST_RUN_ON_FAIL);
}

START_TEST(test_restore_script_pre_post)
{
	const char *buf=MIN_CLIENT_CONF
		"restore_script_pre=pre_path\n"
		"restore_script_pre_arg=pre_arg1\n"
		"restore_script_pre_arg=pre_arg2\n"
		"restore_script_post=post_path\n"
		"restore_script_post_arg=post_arg1\n"
		"restore_script_post_arg=post_arg2\n"
		"restore_script_post_run_on_fail=1\n"
	;
	restore_script_pre_post_checks(buf, "pre_path", "post_path", "pre_arg1",
		"pre_arg2", "post_arg1", "post_arg2");
}
END_TEST

// Same as test_restore_script_pre_post, but use restore_script to set both pre
// and post at the same time.
START_TEST(test_restore_script)
{
	const char *buf=MIN_CLIENT_CONF
		"restore_script=path\n"
		"restore_script_arg=arg1\n"
		"restore_script_arg=arg2\n"
		"restore_script_run_on_fail=1\n"
		"restore_script_post_run_on_fail=1\n"
	;
	restore_script_pre_post_checks(buf, "path", "path", "arg1",
		"arg2", "arg1", "arg2");
}
END_TEST

Suite *suite_conffile(void)
{
	Suite *s;
	TCase *tc_core;

	s=suite_create("conffile");

	tc_core=tcase_create("Core");

	tcase_add_test(tc_core, test_conf_get_pair);
	tcase_add_test(tc_core, test_client_conf);
	tcase_add_test(tc_core, test_client_includes_excludes);
	tcase_add_test(tc_core, test_client_include_failures);
	tcase_add_test(tc_core, test_server_conf);
	tcase_add_test(tc_core, test_server_script_pre_post);
	tcase_add_test(tc_core, test_server_script);
	tcase_add_test(tc_core, test_backup_script_pre_post);
	tcase_add_test(tc_core, test_backup_script);
	tcase_add_test(tc_core, test_restore_script_pre_post);
	tcase_add_test(tc_core, test_restore_script);
	suite_add_tcase(s, tc_core);

	return s;
}