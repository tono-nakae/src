/*	$NetBSD: conf.h,v 1.1 2000/08/12 22:58:10 wdk Exp $	*/

#define mmread mmrw
#define mmwrite mmrw

cdev_decl(mm);

bdev_decl(sw);
cdev_decl(sw);

bdev_decl(raid);
cdev_decl(raid);

bdev_decl(fd);
cdev_decl(fd);

cdev_decl(zs);
cdev_decl(fb);
cdev_decl(bmcn);
cdev_decl(ms);

cdev_decl(scsibus);
