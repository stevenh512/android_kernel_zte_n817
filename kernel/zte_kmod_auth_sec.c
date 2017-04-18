/*
 *  Copyright (C) 2013 Jia.Jia ZTE Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/*
 * Date         Author           Comment
 * -----------  --------------   --------------------------------------------
 * 2013-12-21   Jia              add support for memory reserved in .dts
 * 2013-06-09   Jia              created by ZTE_JIA_20130609 jia.jia
 * --------------------------------------------------------------------------
 */

#if defined(ZTE_FEATURE_TF_SECURITY_SYSTEM_HIGH)

#include <linux/export.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/memory_alloc.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/elf.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/capability.h>
#include <linux/moduleparam.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/sysdev.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/crypto.h>
#include <linux/of.h>
#include <asm/io.h>
#include <crypto/hash.h>
#include <crypto/sha.h>
#include <mach/memory.h>
#include <mach/scm.h>

/*
 * Macro Definition
 */
/*
 * Use reserved memory as default,
 * due to insufficient memory allocated by kzalloc
 */
#undef AUTH_SEC_USE_MEM_RESERVED

#define AUTH_SEC_VERSION  "1.0"
#ifdef AUTH_SEC_USE_MEM_RESERVED
#define AUTH_SEC_MEM_COMPAT_STR  "qcom,kmod-auth-sec-mem"
#endif
#define AUTH_SEC_CLASS_NAME  "zte_kmod_auth_sec"

#define AUTH_SEC_PHNUM  (3)
#define MI_BOOT_IMG_HDR_SIZE  (40)
#define SHA256_SIGNATURE_SIZE  (256)
#define CERT_CHAIN_MAXSIZE  (6 * 1024)
#define MAX_ELF_DATA_HASH_SIZE  (1024)

/*
 * Type Definition
 */
// Refer to arch/arm/mach-msm/scm-pas.h
enum pas_id {
	PAS_MODEM,
	PAS_Q6,
	PAS_DSPS,
	PAS_TZAPPS,
	PAS_MODEM_SW,
	PAS_MODEM_FW,
	PAS_WCNSS,
	PAS_SECAPP,
	PAS_GSS,
	PAS_VIDC,
	PAS_KMOD_AUTH_SEC,
};

typedef struct {
	Elf32_Ehdr ehdr;  // header of ELF
	Elf32_Phdr *phdr; // array of program headers of ELF

	uint32_t certst;  // signature & cert chain status
} auth_sec_file_t;

#ifdef AUTH_SEC_USE_MEM_RESERVED
typedef struct {
	phys_addr_t phys;
	void *virt;
	int32_t size;
} auth_sec_mem_t;
#endif

/*
 * Global Variables Definition
 */
static auth_sec_file_t auth_sec_file;
static struct sys_device auth_sec_sysdev;

#ifdef AUTH_SEC_USE_MEM_RESERVED
static auth_sec_mem_t auth_sec_mem;
#endif

DEFINE_MUTEX(auth_sec_lock);

/*
 * Function declaration
 */
extern int pas_init_image(enum pas_id id, const uint8_t *metadata, size_t size);

static int32_t auth_sec_chk_elf_hdr(const Elf32_Ehdr *ehdr);
static int32_t auth_sec_populate_elf_ehdr(const void __user *umod, auth_sec_file_t *as);
static int32_t auth_sec_populate_elf_phdr_tbl(const void __user *umod, auth_sec_file_t *as);
static int32_t auth_sec_gen_hash(struct crypto_shash *shash, const uint8_t *data, uint32_t len, uint8_t *hash);
static int32_t auth_sec_cert_ehdr_phdrtbl_hash(const void __user *umod, const auth_sec_file_t *as);
static int32_t auth_sec_cert_codeseg_hash(const void __user *umod, const auth_sec_file_t *as);
static int32_t auth_sec_cert_hash(const void __user *umod, const auth_sec_file_t *as);
static int32_t auth_sec_cert_sig_certchain(const void __user *umod, uint32_t len, const auth_sec_file_t *as);

static ssize_t auth_sec_show_certst(struct sys_device *dev, struct sysdev_attribute *attr, char *buf);
static int32_t auth_sec_register_sysdev(struct sys_device *sysdev);

#ifdef AUTH_SEC_USE_MEM_RESERVED
static int32_t auth_sec_mem_probe(struct platform_device *pdev);
#endif

/*
 * Function Definition
 */
/*
 * Check ELF header
 */
static int32_t auth_sec_chk_elf_hdr(const Elf32_Ehdr *ehdr)
{
	if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0
		|| ehdr->e_ident[EI_CLASS] != ELFCLASS32
		|| ehdr->e_ident[EI_VERSION] != EV_CURRENT
		|| ehdr->e_ehsize != sizeof(Elf32_Ehdr)
		|| ehdr->e_phentsize != sizeof(Elf32_Phdr)
		|| ehdr->e_phnum != AUTH_SEC_PHNUM) {
		return -EINVAL;
	}

	return 0;
}

/*
 * Populate ELF header
 */
static int32_t auth_sec_populate_elf_ehdr(const void __user *umod, auth_sec_file_t *as)
{
	if (copy_from_user((void *)&as->ehdr, umod, sizeof(Elf32_Ehdr)) != 0
			|| auth_sec_chk_elf_hdr(&as->ehdr) != 0) {
		return -EFAULT;
	}

	return 0;
}

/*
 * Populate ELF program header table
 */
static int32_t auth_sec_populate_elf_phdr_tbl(const void __user *umod, auth_sec_file_t *as)
{
	const uint8_t __user *ptr = (const uint8_t __user *)umod;

	if (copy_from_user((void *)as->phdr, ptr + as->ehdr.e_phoff, as->ehdr.e_phentsize * as->ehdr.e_phnum) != 0) {
		return -EFAULT;
	}

	return 0;
}

/*
 * Generate hash
 */
static int32_t auth_sec_gen_hash(struct crypto_shash *shash, const uint8_t *data, uint32_t len, uint8_t *hash)
{
	struct shash_desc *desc = NULL;

	desc = (struct shash_desc *)kzalloc(sizeof(*desc) + crypto_shash_descsize(shash), GFP_KERNEL);
	if (!desc) {
		return -ENOMEM;
	}

	desc->tfm = shash;
	desc->flags = CRYPTO_TFM_REQ_MAY_SLEEP;

	crypto_shash_init(desc);
	crypto_shash_update(desc, data, len);
	crypto_shash_final(desc, hash);

	kfree(desc);

	return 0;

}

/*
 * Certify hash for ELF head & program header table
 */
static int32_t auth_sec_cert_ehdr_phdrtbl_hash(const void __user *umod, const auth_sec_file_t *as)
{
	const uint8_t __user *ptr = (const uint8_t __user *)umod;
	struct crypto_shash *shash = NULL;
	uint8_t *buf = NULL;
	uint32_t buf_offset = 0, buf_len = 0;
	uint8_t hash[SHA256_DIGEST_SIZE];
	int32_t ret = 0;

	/*
	 * Generate hash for ELF head & program header table
	 */
	buf_offset = 0;

	buf_len = as->ehdr.e_ehsize + (as->ehdr.e_phnum * as->ehdr.e_phentsize);
	buf = (uint8_t *)kzalloc(buf_len, GFP_KERNEL);
	if (!buf) {
		return -ENOMEM;
	}

	if (copy_from_user((void *)buf, ptr + buf_offset, buf_len) != 0) {
		ret = -EFAULT;
		goto auth_sec_cert_hash_ehdr_phdrtbl_done;
	}

	shash = crypto_alloc_shash("sha256", 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(shash)) {
		ret = PTR_ERR(shash);
		goto auth_sec_cert_hash_ehdr_phdrtbl_done;
	}

	memset((void *)hash, 0, SHA256_DIGEST_SIZE);

	ret = auth_sec_gen_hash(shash, buf, buf_len, hash);
	if (ret != 0) {
		goto auth_sec_cert_hash_ehdr_phdrtbl_done;
	}

	/*
	 * Populate hash of ELF head & program header table
	 */
	buf_offset = as->phdr[AUTH_SEC_PHNUM - 2].p_offset + MI_BOOT_IMG_HDR_SIZE;

	buf_len = SHA256_DIGEST_SIZE;
	buf = (uint8_t *)krealloc(buf, buf_len, GFP_KERNEL);
	if (!buf) {
		ret = -ENOMEM;
		goto auth_sec_cert_hash_ehdr_phdrtbl_done;
	}
	memset((void *)buf, 0, buf_len);

	if (copy_from_user((void *)buf, ptr + buf_offset, buf_len) != 0) {
		ret = -EFAULT;
		goto auth_sec_cert_hash_ehdr_phdrtbl_done;
	}

	/*
	 * Verify hash for ELF head & program header table
	 */
	if (0 != memcmp((const void *)buf, (const void *)hash, SHA256_DIGEST_SIZE)) {
		ret = -EINVAL;
		goto auth_sec_cert_hash_ehdr_phdrtbl_done;
	}

auth_sec_cert_hash_ehdr_phdrtbl_done:

	if (buf) {
		kfree(buf);
	}

	if (shash && !IS_ERR(shash)) {
		crypto_free_shash(shash);
	}

	return ret;
}

/*
 * Certify hash for ELF code segment
 *
 * Hash part, not all, of code segment due to high memory usage,
 * set by 'MAX_ELF_DATA_HASH_SIZE'
 */
static int32_t auth_sec_cert_codeseg_hash(const void __user *umod, const auth_sec_file_t *as)
{
	const uint8_t __user *ptr = (const uint8_t __user *)umod;
	struct crypto_shash *shash = NULL;
	uint8_t *buf = NULL;
	uint32_t buf_offset = 0, buf_len = 0;
	uint8_t *data = NULL;
	uint32_t data_offset = 0, data_len = 0;
	uint8_t hash[SHA256_DIGEST_SIZE];
	int32_t ret = 0;

	/*
	 * Generate hash for code segment
	 */
	buf_offset = as->phdr[AUTH_SEC_PHNUM - 1].p_offset;

#if 0
	buf_len = as->phdr[AUTH_SEC_PHNUM - 1].p_filesz > MAX_ELF_DATA_HASH_SIZE ?
						MAX_ELF_DATA_HASH_SIZE : as->phdr[AUTH_SEC_PHNUM - 1].p_filesz;
#else
        buf_len = as->phdr[AUTH_SEC_PHNUM - 1].p_filesz;
#endif
	buf = (uint8_t *)vmalloc(buf_len);
	if (!buf) {
                pr_err("%s: failed to allocate memory!\n", __func__);
		return -ENOMEM;
	}

	if (copy_from_user((void *)buf, ptr + buf_offset, buf_len) != 0) {
		ret = -EFAULT;
		goto auth_sec_cert_code_seg_done;
	}

	shash = crypto_alloc_shash("sha256", 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(shash)) {
		ret = PTR_ERR(shash);
		goto auth_sec_cert_code_seg_done;
	}

	memset((void *)hash, 0, SHA256_DIGEST_SIZE);

	ret = auth_sec_gen_hash(shash, buf, buf_len, hash);
	if (ret != 0) {
		goto auth_sec_cert_code_seg_done;
	}

	/*
	 * Populate hash of code segment
	 */
	data_offset = as->phdr[AUTH_SEC_PHNUM - 2].p_offset + MI_BOOT_IMG_HDR_SIZE + (SHA256_DIGEST_SIZE * 2);

	data_len = SHA256_DIGEST_SIZE;
	data = (uint8_t *)kzalloc(data_len, GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto auth_sec_cert_code_seg_done;
	}

	if (copy_from_user((void *)data, ptr + data_offset, data_len) != 0) {
		ret = -EFAULT;
		goto auth_sec_cert_code_seg_done;
	}

	/*
	 * Verify hash for code segment
	 */
	if (0 != memcmp((const void *)data, (const void *)hash, SHA256_DIGEST_SIZE)) {
		ret = -EINVAL;
		goto auth_sec_cert_code_seg_done;
	}

auth_sec_cert_code_seg_done:

	if (data) {
		kfree(data);
	}

	if (buf) {
		vfree(buf);
	}

	if (shash && !IS_ERR(shash)) {
		crypto_free_shash(shash);
	}

	return ret;
}

/*
 * Certify hash segments
 */
static int32_t auth_sec_cert_hash(const void __user *umod, const auth_sec_file_t *as)
{
	int32_t ret = 0;

	/*
	 * Certify hash for ELF head & program header table
	 */
	ret = auth_sec_cert_ehdr_phdrtbl_hash(umod, as);
	if (ret != 0) {
		return ret;
	}

	/*
	 * Certify hash for hash table itself
	 */
	// Do nothing here

	/*
	 * Certify hash for ELF code segment
	 */
	ret = auth_sec_cert_codeseg_hash(umod, as);
	if (ret != 0) {
		return ret;
	}

	return 0;
}

/*
 * Certify signature & cert chain
 */
#ifdef AUTH_SEC_USE_MEM_RESERVED
static int32_t auth_sec_cert_sig_certchain(const void __user *umod, uint32_t len, const auth_sec_file_t *as)
{
	void *mdata_buf = NULL;
	int32_t ret = 0;

	/*
	 * Make memory physically contiguous
	 */
	if (!auth_sec_mem.virt || auth_sec_mem.size <= 0 || len > auth_sec_mem.size) {
		pr_err("%s: failed to allocate memory!\n", __func__);
		return -ENOMEM;
	}
	mdata_buf = (void *)auth_sec_mem.virt;
	memset((void *)mdata_buf, 0, len);

	if (copy_from_user((void *)mdata_buf, umod, (unsigned long)len) != 0) {
		return -EFAULT;
	}

	ret = pas_init_image(PAS_KMOD_AUTH_SEC, (const uint8_t *)mdata_buf, len);

	return ret;
}
#else
static int32_t auth_sec_cert_sig_certchain(const void __user *umod, uint32_t len, const auth_sec_file_t *as)
{
	void *mdata_buf = NULL;
	int32_t ret = 0;

	/*
	 * Make memory physically contiguous
	 * however, use vmalloc instead of kzalloc here due to dma_alloc_attrs used in 'pas_init_image'
	 */
	mdata_buf = vmalloc(len);
	if (!mdata_buf) {
		pr_err("%s: failed to allocate memory!\n", __func__);
		return -ENOMEM;
	}

	if (copy_from_user((void *)mdata_buf, umod, (unsigned long)len) != 0) {
		ret = -EFAULT;
		goto auth_sec_cert_sig_certchain_done;
	}

	ret = pas_init_image(PAS_KMOD_AUTH_SEC, (const uint8_t *)mdata_buf, len);

auth_sec_cert_sig_certchain_done:

	if (mdata_buf) {
		vfree(mdata_buf);
		mdata_buf = NULL;
	}

	return ret;
}
#endif /* AUTH_SEC_USE_MEM_RESERVED */

/*
 * Verify auth-sec Kmod
 */
int32_t verify_kmod_auth_sec(void __user *umod, unsigned long len)
{
	int32_t ret = 0;

	pr_debug("%s: e\n", __func__);

	/*
	 * Sanity check
	 */
	if (len < sizeof(Elf32_Ehdr)) {
		pr_err("%s: invalid values!\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&auth_sec_lock);

	/*
	 * Populate ELF header
	 */
	ret = auth_sec_populate_elf_ehdr((const void __user *)umod, &auth_sec_file);
	if (ret != 0) {
		pr_err("%s: failed to populate ELF header!\n", __func__);
		goto verify_kmod_auth_sec_done;
	}

	/*
	 * Populate ELF program header table
	 */
	auth_sec_file.phdr = (Elf32_Phdr *)vmalloc(auth_sec_file.ehdr.e_phentsize * auth_sec_file.ehdr.e_phnum);
	if (!auth_sec_file.phdr) {
		pr_err("%s: no enough memory!\n", __func__);
		ret = -ENOMEM;
		goto verify_kmod_auth_sec_done;
	}

	ret = auth_sec_populate_elf_phdr_tbl((const void __user *)umod, &auth_sec_file);
	if (ret != 0) {
		pr_err("%s: failed to populate ELF program header table!\n", __func__);
		goto verify_kmod_auth_sec_done;
	}

	/*
	 * Certify signature & cert chain
	 */
	/*
	 * len = sizeof ELF header + sizeof Program Header Table + sizeof Hash Table(Segment 0)
	 *
	 * Kmod ELF(Segment 1) NOT required here
	 */
	len = auth_sec_file.phdr[AUTH_SEC_PHNUM - 1].p_offset;

	ret = auth_sec_cert_sig_certchain((const void __user *)umod, len, &auth_sec_file);
	if (ret != 0) {
		pr_err("%s: failed to certify ELF signature & cert chain!\n", __func__);
		goto verify_kmod_auth_sec_done;
	}

	/*
	 * Certify hash segments
	 */
	ret = auth_sec_cert_hash((const void __user *)umod, &auth_sec_file);
	if (ret != 0) {
		pr_err("%s: failed to certify ELF hash table!\n", __func__);
		goto verify_kmod_auth_sec_done;
	}

verify_kmod_auth_sec_done:

	if (auth_sec_file.phdr) {
		vfree(auth_sec_file.phdr);
		auth_sec_file.phdr = NULL;
	}

	mutex_unlock(&auth_sec_lock);

	pr_debug("%s: x\n", __func__);

	return ret;
}
EXPORT_SYMBOL(verify_kmod_auth_sec);

/*
 * Load real Kmod after auth sec
 */
int32_t load_kmod_auth_sec(void __user **umod, unsigned long *len)
{
	int32_t ret = 0;

	pr_debug("%s: e\n", __func__);

	/*
	 * Sanity check
	 */
	if (*len < sizeof(Elf32_Ehdr)) {
		pr_err("%s: invalid values!\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&auth_sec_lock);

	/*
	 * Populate ELF header
	 */
	ret = auth_sec_populate_elf_ehdr((const void __user *)*umod, &auth_sec_file);
	if (ret != 0) {
		pr_err("%s: failed to populate ELF header!\n", __func__);
		goto load_kmod_auth_sec_done;
	}

	/*
	 * Populate ELF program header table
	 */
	auth_sec_file.phdr = (Elf32_Phdr *)vmalloc(auth_sec_file.ehdr.e_phentsize * auth_sec_file.ehdr.e_phnum);
	if (!auth_sec_file.phdr) {
		pr_err("%s: no enough memory!\n", __func__);
		ret = -ENOMEM;
		goto load_kmod_auth_sec_done;
	}

	ret = auth_sec_populate_elf_phdr_tbl((const void __user *)*umod, &auth_sec_file);
	if (ret != 0) {
		pr_err("%s: failed to populate ELF program header table!\n", __func__);
		goto load_kmod_auth_sec_done;
	}

	/*
	 * Populate ELF code segment which is at index of AUTH_SEC_PHNUM - 1
	 */
	*umod = (uint8_t __force *)(*umod) + auth_sec_file.phdr[AUTH_SEC_PHNUM - 1].p_offset;
	*len = auth_sec_file.phdr[AUTH_SEC_PHNUM - 1].p_filesz;

load_kmod_auth_sec_done:

	if (auth_sec_file.phdr) {
		vfree(auth_sec_file.phdr);
		auth_sec_file.phdr = NULL;
	}

	mutex_unlock(&auth_sec_lock);

	pr_debug("%s: x\n", __func__);

	return ret;
}
EXPORT_SYMBOL(load_kmod_auth_sec);

/*
 * Show status of certificate
 */
static ssize_t auth_sec_show_certst(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	/*
	 * No sanity check here!
	 */
	return snprintf(buf, PAGE_SIZE, "%d\n", auth_sec_file.certst);
}
static SYSDEV_ATTR(certst, S_IRUSR, auth_sec_show_certst, NULL);

static struct sysdev_attribute *auth_sec_attrs[] = {
	&attr_certst,
};

static struct sysdev_class auth_sec_sysdev_class = {
	.name = AUTH_SEC_CLASS_NAME
};

/*
 * Sys device register
 *
 * sysdev file:
 *
 * /sys/devices/system/zte_kmod_auth_sec/zte_kmod_auth_sec0/certst
 */
static int32_t auth_sec_register_sysdev(struct sys_device *sysdev)
{
	int32_t i = 0;
	int32_t ret = 0;

	pr_debug("%s: e\n", __func__);

	ret = sysdev_class_register(&auth_sec_sysdev_class);
	if (ret) {
		pr_err("%s: failed to register sys class!\n", __func__);
		return ret;
	}

	sysdev->id = 0;
	sysdev->cls = &auth_sec_sysdev_class;

	ret = sysdev_register(sysdev);
	if (ret) {
		pr_err("%s: failed to register sys dev!\n", __func__);
		sysdev_class_unregister(&auth_sec_sysdev_class);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(auth_sec_attrs); ++i) {
		ret = sysdev_create_file(sysdev, auth_sec_attrs[i]);
		if (ret) {
			pr_err("%s: failed to create sys dev file!\n", __func__);
			goto auth_sec_register_sysdev_fail;
		}
	}

	pr_debug("%s: x\n", __func__);

	return 0;

auth_sec_register_sysdev_fail:

	while (--i >= 0) sysdev_remove_file(sysdev, auth_sec_attrs[i]);

	sysdev_unregister(sysdev);
	sysdev_class_unregister(&auth_sec_sysdev_class);

	return ret;
}

#ifdef AUTH_SEC_USE_MEM_RESERVED
/*
 * Probe device
 */
static int32_t auth_sec_mem_probe(struct platform_device *pdev)
{
	int32_t size = 0;
	int32_t ret = 0;

	pr_debug("%s: e\n", __func__);

	if (!pdev->dev.of_node) {
		pr_err("%s: invalid of_node!\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32((&pdev->dev)->of_node,
					"qcom,memory-reservation-size",
					&size);
	if (ret < 0) {
		pr_err("%s: invalid of_property!\n", __func__);
		return ret;
	}

	if (size <= 0) {
		pr_err("%s: invalid memory size!\n", __func__);
		return -EINVAL;
	}
	auth_sec_mem.size = size;

	auth_sec_mem.phys = allocate_contiguous_ebi_nomap(auth_sec_mem.size, SZ_4K);
	if (!auth_sec_mem.phys) {
		pr_err("%s: failed to allocate memory!\n", __func__);
		return -ENOMEM;
	}
	pr_info("%s: phys: 0x%p, size: %d\n", __func__, (void *)auth_sec_mem.phys, auth_sec_mem.size);

	auth_sec_mem.virt = ioremap(auth_sec_mem.phys, auth_sec_mem.size);
	if (!auth_sec_mem.virt) {
		pr_err("%s: failed to ioremap!\n", __func__);
		free_contiguous_memory_by_paddr(auth_sec_mem.phys);
		return -ENOMEM;
	}
	memset((void *)auth_sec_mem.virt, 0, auth_sec_mem.size);

	pr_debug("%s: x\n", __func__);

	return 0;
}

static struct of_device_id msm_match_table[] = {
	{ .compatible = AUTH_SEC_MEM_COMPAT_STR },
	{},
};
EXPORT_COMPAT(AUTH_SEC_MEM_COMPAT_STR);

static struct platform_driver auth_sec_mem_driver = {
	.driver         = {
		.name = "kmod_auth_sec_mem",
		.owner = THIS_MODULE,
		.of_match_table = msm_match_table
	},
};
#endif /* AUTH_SEC_USE_MEM_RESERVED */

/*
 * Initializes the module.
 */
static int32_t __init auth_sec_init(void)
{
	int32_t ret = 0;

	pr_debug("%s: e\n", __func__);

	/*
	 * Register device driver
	 */
#ifdef AUTH_SEC_USE_MEM_RESERVED
	ret = platform_driver_probe(&auth_sec_mem_driver, auth_sec_mem_probe);
	if (ret) {
		pr_err("%s: failed to register driver!\n", __func__);
		return ret;
	}
#else
	ret = ret;
#endif

	/*
	 * Register sys device
	 */
	(void)auth_sec_register_sysdev(&auth_sec_sysdev);

	pr_debug("%s: x\n", __func__);

	return 0;
}

/*
 * Cleans up the module.
 */
static void __exit auth_sec_exit(void)
{
	/*
	 * Unregister sysdev
	 */
	// Add code here

	/*
	 * Unregister device driver
	 */
#ifdef AUTH_SEC_USE_MEM_RESERVED
	platform_driver_unregister(&auth_sec_mem_driver);
#endif
}

module_init(auth_sec_init);
module_exit(auth_sec_exit);

MODULE_DESCRIPTION("ZTE Auth-Sec Module Ver %s" AUTH_SEC_VERSION);
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

#endif /* ZTE_FEATURE_TF_SECURITY_SYSTEM_HIGH */

