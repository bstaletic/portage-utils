/*
 * Copyright 2005-2014 Gentoo Foundation
 * Distributed under the terms of the GNU General Public License v2
 *
 * Copyright 2005-2010 Ned Ludd        - <solar@gentoo.org>
 * Copyright 2005-2014 Mike Frysinger  - <vapier@gentoo.org>
 */

#ifdef APPLET_qsearch

#define QSEARCH_FLAGS "acesSNH" COMMON_FLAGS
static struct option const qsearch_long_opts[] = {
	{"all",       no_argument, NULL, 'a'},
	{"cache",     no_argument, NULL, 'c'},
	{"ebuilds",   no_argument, NULL, 'e'},
	{"search",    no_argument, NULL, 's'},
	{"desc",       a_argument, NULL, 'S'},
	{"name-only", no_argument, NULL, 'N'},
	{"homepage",  no_argument, NULL, 'H'},
	COMMON_LONG_OPTS
};
static const char * const qsearch_opts_help[] = {
	"List the descriptions of every package in the cache",
	"Use the portage cache (default)",
	"Use the portage ebuild tree",
	"Regex search package basenames",
	"Regex search package descriptions",
	"Only show package name",
	"Show homepage info",
	COMMON_OPTS_HELP
};
#define qsearch_usage(ret) usage(ret, QSEARCH_FLAGS, qsearch_long_opts, qsearch_opts_help, lookup_applet_idx("qsearch"))

int qsearch_main(int argc, char **argv)
{
	FILE *fp;
	char *ebuild = NULL;
	char last[126] = "";
	char *p, *q, *str;
	char *search_me = NULL;
	char show_homepage = 0, show_name_only = 0;
	char search_desc = 0, search_all = 0, search_name = 1, search_cache = CACHE_EBUILD;
	const char *search_vars[] = { "DESCRIPTION=", "HOMEPAGE=" };
	size_t search_len, ebuild_len;
	int i, idx=0;

	DBG("argc=%d argv[0]=%s argv[1]=%s",
	    argc, argv[0], argc > 1 ? argv[1] : "NULL?");

	while ((i = GETOPT_LONG(QSEARCH, qsearch, "")) != -1) {
		switch (i) {
		COMMON_GETOPTS_CASES(qsearch)
		case 'a': search_all = 1; break;
		case 'c': search_cache = CACHE_METADATA; break;
		case 'e': search_cache = CACHE_EBUILD; break;
		case 's': search_desc = 0; search_name = 1; break;
		case 'S': search_desc = 1; search_name = 0; break;
		case 'N': show_name_only = 1; break;
		case 'H': show_homepage = 1, idx = 1; break;
		}
	}

	if (search_all) {
		search_desc = 1;
		search_name = 0;
	} else {
		if (argc == optind)
			qsearch_usage(EXIT_FAILURE);
		search_me = argv[optind];
	}
#ifdef TESTING
	/* FIXME: hardcoded */
	if ((search_cache == CACHE_EBUILD) && (access("/usr/portage/.qsearch.x", R_OK) == 0)) {
		if ((fp = fopen("/usr/portage/.qsearch.x", "r")) != NULL) {
			search_len = strlen(search_me);
			while (fgets(buf, sizeof(buf), fp) != NULL) {
				if (strlen(buf) <= search_len)
					continue;
				/* add regexp, color highlighting and basename checks */
				if (strncmp(buf, search_me, search_len) == 0) {
					fputs(buf, stdout);
				}
			}
			fclose(fp);
			return 0;
		}
	}
#endif
	last[0] = 0;
	fp = fopen(initialize_flat(search_cache, false), "r");
	if (!fp)
		return 1;

	int portdir_fd = open(portdir, O_RDONLY|O_CLOEXEC|O_PATH);
	if (portdir_fd < 0)
		errp("open(%s) failed", portdir);

	q = NULL; /* Silence a gcc warning. */
	search_len = strlen(search_vars[idx]);

	while (getline(&ebuild, &ebuild_len, fp) != -1) {
		if ((p = strchr(ebuild, '\n')) != NULL)
			*p = 0;
		if (!ebuild[0])
			continue;

		switch (search_cache) {

		case CACHE_METADATA: {
			portage_cache *pcache;
			if ((pcache = cache_read_file(ebuild)) != NULL) {
				if (strcmp(pcache->atom->PN, last) != 0) {
					strncpy(last, pcache->atom->PN, sizeof(last));
					if ((rematch(search_me, (search_desc ? pcache->DESCRIPTION : ebuild), REG_EXTENDED | REG_ICASE) == 0) || search_all)
						printf("%s%s/%s%s%s %s\n", BOLD, pcache->atom->CATEGORY, BLUE,
						       pcache->atom->PN, NORM,
						       (show_name_only ? "" :
						        (show_homepage ? pcache->HOMEPAGE : pcache->DESCRIPTION)));
				}
				cache_free(pcache);
			} else {
				if (!reinitialize)
					warnf("(cache update pending) %s", ebuild);
				reinitialize = 1;
			}
			break;
		}

		case CACHE_EBUILD: {
			FILE *ebuildfp;
			str = xstrdup(ebuild);
			p = dirname(str);

			if (strcmp(p, last) != 0) {
				bool show_it = false;
				strncpy(last, p, sizeof(last));
				if (search_name) {
					if (rematch(search_me, basename(last), REG_EXTENDED | REG_ICASE) != 0) {
						goto no_cache_ebuild_match;
					} else {
						q = NULL;
						show_it = true;
					}
				}

				int fd = openat(portdir_fd, ebuild, O_RDONLY|O_CLOEXEC);
				if (fd != -1) {
					ebuildfp = fdopen(fd, "r");
					if (ebuildfp == NULL) {
						close(fd);
						continue;
					}
				} else {
					if (!reinitialize)
						warnfp("(cache update pending) %s", ebuild);
					reinitialize = 1;
					goto no_cache_ebuild_match;
				}

				char *buf = NULL;
				size_t buflen;
				while (getline(&buf, &buflen, ebuildfp) != -1) {
					if (strlen(buf) <= search_len)
						continue;
					if (strncmp(buf, search_vars[idx], search_len) != 0)
						continue;
					if ((q = strrchr(buf, '"')) != NULL)
						*q = 0;
					if (strlen(buf) <= search_len)
						break;
					q = buf + search_len + 1;
					if (!search_all && !search_name && rematch(search_me, q, REG_EXTENDED | REG_ICASE) != 0)
						break;
					show_it = true;
					break;
				}

				if (show_it) {
					printf("%s%s/%s%s%s %s\n",
						BOLD, dirname(p), BLUE, basename(p), NORM,
						(show_name_only ? "" : q ? : "<no DESCRIPTION found>"));
				}

				free(buf);
				fclose(ebuildfp);
			}
no_cache_ebuild_match:
			free(str);

			break;
		} /* case CACHE_EBUILD */
		} /* switch (search_cache) */
	}
	free(ebuild);
	close(portdir_fd);
	fclose(fp);
	return EXIT_SUCCESS;
}

#else
DEFINE_APPLET_STUB(qsearch)
#endif
