/*
 * Copyright 2005-2006 Gentoo Foundation
 * Distributed under the terms of the GNU General Public License v2
 * $Header: /var/cvsroot/gentoo-projects/portage-utils/qmerge.c,v 1.9 2006/01/07 23:18:07 solar Exp $
 *
 * Copyright 2005-2006 Ned Ludd        - <solar@gentoo.org>
 * Copyright 2005-2006 Mike Frysinger  - <vapier@gentoo.org>
 */

#ifdef APPLET_qmerge

#define QMERGE_FLAGS "flipyF" COMMON_FLAGS
static struct option const qmerge_long_opts[] = {
	{"fetch",     no_argument, NULL, 'f'},
	{"list",      no_argument, NULL, 'l'},
	{"install",   no_argument, NULL, 'i'},
	{"pretend",   no_argument, NULL, 'p'},
	{"yes",       no_argument, NULL, 'y'},
        COMMON_LONG_OPTS
};
static const char *qmerge_opts_help[] = {
	"force download overwriting any existing files",
	"list available packages",
	"install package",
	"pretend only",
	"dont prompt before overwriting",
        COMMON_OPTS_HELP
};

static const char qmerge_rcsid[] = "$Id: qmerge.c,v 1.9 2006/01/07 23:18:07 solar Exp $";
#define qmerge_usage(ret) usage(ret, QMERGE_FLAGS, qmerge_long_opts, qmerge_opts_help, lookup_applet_idx("qmerge"))

char pretend = 0;
char list_packages = 0;
char interactive = 1;
char install = 0;
char force_download = 0;

struct pkg_t {
	char PF[64];
	char CATEGORY[64];
	char DESC[126];
	char LICENSE[64];
	char RDEPEND[BUFSIZ];
	char MD5[32];
	/* char SHA1[40]; */
	char SLOT[64];
	size_t SIZE;
	char USE[BUFSIZ];
} Pkg;

int interactive_rename(const char *, const char *);
int interactive_rename(const char *src, const char *dst) {
	char buf[1024];
#if 0
	struct stat st;

	if (stat(dst, &st) == (-1))
		warn("%s does exist", dst);
	else
		warn("%s exists", dst);
#endif
	snprintf(buf, sizeof(buf), "/bin/busybox mv %s %s %s", interactive ? "-i" : "", src, dst);
	system(buf);
	if (verbose) printf("%s>>>%s %s\n", GREEN, NORM, dst);

	return 0;
}

void fetch(const char *, const char *);
void fetch(const char *destdir, const char *src) {
	char buf[BUFSIZ];
	snprintf(buf, sizeof(buf), "%s/bin/busybox wget %s -P %s %s/%s", force_download ? "" : pretend ? "echo " : "",
		(quiet ? "-q" : ""), destdir, binhost, src);
	system(buf);
}

void qmerge_initialize(const char *);
void qmerge_initialize(const char *Packages) {
	if (pkgdir[0] == '/') {
		int len = strlen(pkgdir);
		if (len > 5) {
			if ((strcmp(&pkgdir[len-4], "/All")) != 0)
				strncat(pkgdir, "/All", sizeof(pkgdir));
		} else
			errf("PKGDIR=%s is to short to be valid", pkgdir);
	}

	if (!binhost[0])
		errf("PORTAGE_BINHOST= does not appear to be valid");
	
	if ((access(pkgdir, R_OK|W_OK|X_OK)) != 0)
		errf("Fatal errors with PKGDIR='%s'", pkgdir);

	mkdir(port_tmpdir, 0755);

	if (chdir(port_tmpdir) != 0)
		errf("!!! chdir(PORTAGE_TMPDIR %s) %s", port_tmpdir, strerror(errno));
	if (force_download)
		unlink(Packages);
	if (access(Packages, R_OK) != 0)
		fetch("./", Packages);
}

char *best_version(depend_atom *);
char *best_version(depend_atom *atom) {
	static char buf[1024];
	FILE *fp;
	char *p;

	snprintf(buf, sizeof(buf), "qlist -CIev %s/%s", atom->CATEGORY, atom->PN);
	if ((fp = popen(buf, "r")) == NULL)
		return NULL;

	buf[0] = '\0';
	if ((fgets(buf, sizeof(buf), fp)) != NULL)
		if ((p = strchr(buf, '\n')) != NULL)
			*p = 0;
	pclose(fp);
	return (char *) buf;
}


/* oh shit getting into pkg mgt here. wishlist vercmp() function */
void pkg_merge(depend_atom *);
void pkg_merge(depend_atom *atom) {
	FILE *fp, *contents;
	char buf[1024];
	char tarball[255];
	char installed_version[126];
	char *p;
	int i;
	char **ARGV = NULL;
	int ARGC = 0;

	if (!install) return;

	if (chdir(port_tmpdir) != 0) errf("!!! chdir(%s) %s", port_tmpdir, strerror(errno));

	mkdir(Pkg.PF, 0710);
	if (chdir(Pkg.PF) != 0) errf("!!! chdir(%s) %s", Pkg.PF, strerror(errno));

	/* check for an already install pkg */
	snprintf(buf, sizeof(buf), "%s/%s", atom->CATEGORY, Pkg.PF);
	p = best_version(atom);

	if (*p) {
		strncpy(installed_version, p, sizeof(installed_version));
	} else  {
		installed_version[0] = 0;
	}
	
	/* split the tbz and xpak data */
	snprintf(tarball, sizeof(tarball), "%s.tbz2", Pkg.PF);
	snprintf(buf, sizeof(buf), "%s/%s", pkgdir, tarball);
	unlink(tarball);
	symlink(buf, tarball);
	snprintf(buf, sizeof(buf), "q tbz2 -s %s", tarball);
	system(buf);

	mkdir("vdb", 0755);
	mkdir("image", 0755);

	/* list and extract vdb files from the xpak */
	snprintf(buf, sizeof(buf), "q xpak -d %s/%s/vdb -x %s.xpak `q xpak -l %s.xpak`", 
		port_tmpdir, Pkg.PF, Pkg.PF, Pkg.PF);
	system(buf);

	/* extrct the binary package data */
	snprintf(buf, sizeof(buf), "/bin/busybox tar -jx%sf %s.tar.bz2 -C image/", ((verbose > 1) ? "v" : ""), Pkg.PF);
	system(buf);
	fflush(stdout);

	/* check for an already install pkg */
	snprintf(buf, sizeof(buf), "%s/%s", atom->CATEGORY, Pkg.PF);

	if (installed_version[0]) {
		if ((strcmp(installed_version, buf)) == 0) {
			if (verbose) 
				fprintf(stderr, "%s already installed\n", installed_version);
		} else {
			if (verbose)
				printf("local: %s remote %s\n", installed_version, buf);
		}
	} else {
		if (verbose)
			printf("Installing %s\n", buf);
	}

	if ((contents = fopen("vdb/CONTENTS", "w")) == NULL)
		errf("come on wtf?");

	chdir("image");
	if ((fp = popen("/bin/busybox find .", "r")) == NULL)
		errf("come on wtf!");

	makeargv(config_protect, &ARGC, &ARGV);

	while ((fgets(buf, sizeof(buf), fp)) != NULL) {
		struct stat st;
		char line[BUFSIZ];

		if ((p = strrchr(buf, '\n')) != NULL)
			*p = 0;
		if (buf[0] != '.')
			continue;

		if (((strcmp(buf, ".")) == 0) || ((strcmp(buf, "..")) == 0))
			continue;

		/* use lstats for symlinks */
		lstat(buf, &st);

		line[0] = 0;

		if (pretend) continue;

		if (S_ISDIR(st.st_mode)) {
			mkdir(&buf[1], st.st_mode);
			chown(&buf[1], st.st_uid, st.st_gid);
			snprintf(line, sizeof(line), "dir %s", &buf[1]);
		}
		if (S_ISREG(st.st_mode)) {
			struct timeval tv;
			char *hash;
			int protected = 0;

			hash = hash_file(buf, HASH_MD5);
			// sha1_hash = hash_file(buf, HASH_SHA1);

			snprintf(line, sizeof(line), "obj %s %s %lu", &buf[1], hash, st.st_mtime);
			/* /etc /usr/kde/2/share/config /usr/kde/3/share/config	/var/qmail/control */

			for (i = 1; i < ARGC; i++)
				if ((strncmp(ARGV[i], &buf[1], strlen(ARGV[i]))) == 0)
					if ((access(&buf[1], R_OK)) == 0)
						protected = 1;

			if ((strncmp("/etc/", &buf[1], 5)) == 0)
				if ((access(&buf[1], R_OK)) == 0)
					protected = 1;

			if (protected) {
				printf("CFG: %s\n", &buf[1]);
				continue;
			}

			if (interactive_rename(buf, &buf[1]) != 0)
				continue;

			chmod(&buf[1], st.st_mode);
			chown(&buf[1], st.st_uid, st.st_gid);

			tv.tv_sec = st.st_mtime;
			tv.tv_usec = st.st_mtime;
			// utimes(&buf[1], &tv);

		}
		if (S_ISLNK(st.st_mode)) {
			char path[sizeof(buf)];
			memset(&path, 0, sizeof(path));
			/* symlinks are unfinished */
			readlink(buf, path, sizeof(path));
			snprintf(line, sizeof(line), "sym %s -> %s", &buf[1], path);
			warnf("%s", line);
			assert(strlen(path));;
			assert(strlen(&buf[1]));
		}
		/* Save the line to the contents file */
		if (*line) fprintf(contents, "%s\n", line);
	}
                                                
	if (ARGC > 0) {
		for (i = 0; i < ARGC; i++)
			free(ARGV[i]);
		free(ARGV);
	}

	fclose(contents);
	fclose(fp);

	chdir(port_tmpdir);
	chdir(Pkg.PF);

	if (!pretend) {
		snprintf(buf, sizeof(buf), "/var/db/pkg/%s/", Pkg.CATEGORY);
		if (access(buf, R_OK|W_OK|X_OK) != 0) {
			mkdir("/var", 0755);
			mkdir("/var/db", 0755);
			mkdir("/var/db/pkg/", 0755);
			mkdir(buf, 0755);
		}
		strncat(buf, Pkg.PF, sizeof(buf));
		/* not quiet perfect when a version is already installed */
		if (access(buf, X_OK) == 0) {
			char buf2[sizeof(buf)] = "";
			snprintf(buf2, sizeof(buf2), "rm -rf %s", buf);
			system(buf2);
		}
		interactive_rename("vdb", buf);
		
	}
	chdir(port_tmpdir);
}

int pkg_unmerge(char *);
int pkg_unmerge(char *pkg) {
	return 1;
}

int unlink_empty(char *);
int unlink_empty(char *buf) {
	struct stat st;
	if ((stat(buf, &st)) != (-1))
		if (st.st_size == 0)
			return unlink(buf);
	return (-1);
}

void pkg_fetch(int, char **);
void pkg_fetch(int argc, char **argv) {
	depend_atom *atom;
	char buf[255];
	char buf2[255];
	char *hash;
	int i;

	snprintf(buf, sizeof(buf), "%s/%s", Pkg.CATEGORY, Pkg.PF);
	if ((atom = atom_explode(buf)) == NULL)
		errf("%s/%s is not a valid atom", Pkg.CATEGORY, Pkg.PF);

	snprintf(buf2, sizeof(buf2), "%s/%s", Pkg.CATEGORY, atom->PN);

	for (i = 1; i < argc; i++) {
		int match = 0;

		if (argv[i][0] == '-')
			continue;

		/* verify this is the requested package */
		if ((strcmp(argv[i], buf)) == 0)
			match = 1;
		if ((strcmp(argv[i], Pkg.PF)) == 0)
			match = 1;
	 	if ((strcmp(argv[i], atom->PN)) == 0)
			match = 1;

		if (match != 1)
			continue;

		/* check to see if file exists and it's checksum matches */
		snprintf(buf, sizeof(buf), "%s/%s.tbz2", pkgdir, Pkg.PF);
		unlink_empty(buf);
		if (force_download) unlink(buf);

		if (access(buf, R_OK) == 0) {
			hash = (char*) hash_file(buf, HASH_MD5);
			if (strcmp(hash, Pkg.MD5) == 0) {
				printf("MD5: [%sOK%s] %s %s/%s\n", GREEN, NORM, hash, atom->CATEGORY, Pkg.PF);
				/* attempt to merge it */
				pkg_merge(atom);
				continue;
			}
		}
		if (verbose)
			printf("Fetching %s/%s.tbz2\n", atom->CATEGORY, Pkg.PF);

		fflush(stdout);
		fflush(stderr);

		/* fetch the package */
		snprintf(buf, sizeof(buf), "%s.tbz2", Pkg.PF);
		fetch(pkgdir, buf);

		fflush(stdout);
		fflush(stderr);

		/* verify the pkg exists now. unlink if zero bytes */
		snprintf(buf, sizeof(buf), "%s/%s.tbz2", pkgdir, Pkg.PF);
		unlink_empty(buf);

		if (access(buf, R_OK) != 0) {
			warn("Failed to fetch %s.tbz2 from %s", Pkg.PF, binhost);
			fflush(stderr);
			continue;
		}

		/* verify it's checksum */
		hash = (char*) hash_file(buf, HASH_MD5);
		if ((strcmp(hash, Pkg.MD5)) != 0) {
			printf("MD5: [%sER%s] %s != %s %s/%s\n", RED, NORM, hash, Pkg.MD5, atom->CATEGORY, Pkg.PF);
			continue;
		}

		printf("MD5: [%sOK%s] %s %s/%s\n", GREEN, NORM, hash, atom->CATEGORY, Pkg.PF);
		fflush(stdout);

		/* attempt to merge it */
		pkg_merge(atom);
	}
	/* free the atom */
	atom_implode(atom);
}

void print_Pkg(int);
void print_Pkg(int full) {

	if (!Pkg.CATEGORY[0]) errf("CATEGORY is NULL");
	if (!Pkg.PF[0]) errf("PF is NULL");

	printf("%s%s/%s%s%s%s%s%s\n", BOLD, Pkg.CATEGORY, BLUE, Pkg.PF, NORM,
		!quiet ? " [" : "", 
		!quiet ? make_human_readable_str(Pkg.SIZE, 1, KILOBYTE) : "",
		!quiet ? "KB]" : "");

	if (full == 0)
		return;

	if (Pkg.DESC[0])
		printf("  %sDesc%s:%s      %s\n", MAGENTA, YELLOW, NORM, Pkg.DESC);
	if (Pkg.LICENSE[0])
		printf("  %sLicense%s:%s   %s\n", MAGENTA, YELLOW, NORM, Pkg.LICENSE);
	if (Pkg.RDEPEND[0])
		printf("  %sRdepend%s:%s   %s\n", MAGENTA, YELLOW, NORM, Pkg.RDEPEND);
	if (Pkg.MD5[0])
		printf("  %sMd5%s:%s       %s\n", MAGENTA, YELLOW, NORM, Pkg.MD5);
	if (Pkg.SLOT[0])
		printf("  %sSlot%s:%s      %s\n", MAGENTA, YELLOW, NORM, Pkg.SLOT);
	if (Pkg.USE[0])
		printf("  %sUse%s:%s       %s\n", MAGENTA, YELLOW, NORM, Pkg.USE);	
}

void parse_packages(const char *, int , char **);
void parse_packages(const char *Packages, int argc, char **argv) {
	FILE *fp;
	char buf[BUFSIZ];
	char value[BUFSIZ];
	char *p;
	int i;
	long lineno = 0;

	if ((fp = fopen(Packages, "r")) == NULL)
		exit(1);

	memset(&Pkg, 0, sizeof(Pkg));

	while((fgets(buf, sizeof(buf), fp)) != NULL) {
		lineno++;
		if (*buf == '\n') {
			if ((strlen(Pkg.PF) > 0) && (strlen(Pkg.CATEGORY) > 0)) {
				if (list_packages) {
					if (argc != optind) {
						for ( i = 0 ; i < argc; i++)
							if ((strncmp(argv[i], Pkg.PF, strlen(argv[i])) == 0) || (strcmp(argv[i], Pkg.CATEGORY) == 0))
								print_Pkg(verbose);
					} else {
						print_Pkg(verbose);
					}
				} else {
					pkg_fetch(argc, argv);
				}
			}
			memset(&Pkg, 0, sizeof(Pkg));
			continue;
		}
		if ((p = strchr(buf, '\n')) != NULL)
			*p = 0;

		memset(&value, 0, sizeof(value));
		if ((p = strchr(buf, ':')) == NULL)
			continue;
		if ((p = strchr(buf, ' ')) == NULL)
			continue;
		*p = 0;
		++p;
		strncpy(value, p, sizeof(value));
		// printf("%s=%s\n", buf, value);

		switch(*buf) {
			case 'U':
				if ((strcmp(buf, "USE:")) == 0) strncpy(Pkg.USE, value, sizeof(Pkg.USE));
				break;
			case 'P':
				if ((strcmp(buf, "PF:")) == 0) strncpy(Pkg.PF, value, sizeof(Pkg.PF));
				break;
			case 'S':
				if ((strcmp(buf, "SIZE:")) == 0) Pkg.SIZE = atol(value);
				if ((strcmp(buf, "SLOT:")) == 0) strncpy(Pkg.SLOT, value, sizeof(Pkg.SLOT));
				break;
			case 'M':
				if ((strcmp(buf, "MD5:")) == 0) strncpy(Pkg.MD5, value, sizeof(Pkg.MD5));
				break;
			case 'R':
				if ((strcmp(buf, "RDEPEND:")) == 0) strncpy(Pkg.RDEPEND, value, sizeof(Pkg.RDEPEND));
				break;
			case 'L':
				if ((strcmp(buf, "LICENSE:")) == 0) strncpy(Pkg.LICENSE, value, sizeof(Pkg.LICENSE));
				break;
			case 'C':
				if ((strcmp(buf, "CATEGORY:")) == 0) strncpy(Pkg.CATEGORY, value, sizeof(Pkg.CATEGORY));
				break;
			case 'D':
				if ((strcmp(buf, "DESC:")) == 0) strncpy(Pkg.DESC, value, sizeof(Pkg.DESC));
				break;
			default:
				fprintf(stderr, "Unhandled parms %s\n", buf);
				break;
		}
	}
	fclose(fp);
}


int qmerge_main(int argc, char **argv) {
	int i;
	const char *Packages = "Packages";
	if (argc < 2)
		qmerge_usage(EXIT_FAILURE);

	while ((i = GETOPT_LONG(QMERGE, qmerge, "")) != -1) {
		switch (i) {
			case 'f': force_download = 1; break;
			case 'l': list_packages = 1; break;
			case 'i': install = 1; break;
			case 'p': list_packages = pretend = 1; break;
			case 'y': interactive = 0; break;
			COMMON_GETOPTS_CASES(qmerge)
		}
	}
	// if (argc == optind) qmerge_usage(EXIT_FAILURE);
	qmerge_initialize(Packages);
	parse_packages(Packages, argc, argv);
	return 0;
}

#else /* ! APPLET_qmerge */
int qmerge_main(int argc, char **argv) {
	errf("%s", err_noapplet);
}
#endif /* APPLET_qmerge */
