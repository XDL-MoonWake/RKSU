static int anon_ksu_release(struct inode *inode, struct file *filp)
{
    pr_info("ksu fd released\n");
    return 0;
}

static long anon_ksu_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    return ksu_supercall_handle_ioctl(cmd, (void __user *)arg);
}

static const struct file_operations anon_ksu_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = anon_ksu_ioctl,
    .compat_ioctl = anon_ksu_ioctl,
    .release = anon_ksu_release,
};

int ksu_install_fd(void)
{
    struct file *filp;
    int fd;

    fd = get_unused_fd_flags(O_CLOEXEC);
    if (fd < 0) {
        pr_err("ksu_install_fd: failed to get unused fd\n");
        return fd;
    }

    filp = anon_inode_getfile("[ksu_driver]", &anon_ksu_fops, NULL, O_RDWR | O_CLOEXEC);
    if (IS_ERR(filp)) {
        pr_err("ksu_install_fd: failed to create anon inode file\n");
        put_unused_fd(fd);
        return PTR_ERR(filp);
    }

    fd_install(fd, filp);
    pr_info("ksu fd installed: %d for pid %d\n", fd, current->pid);
    return fd;
}

struct ksu_install_fd_tw {
    struct callback_head cb;
    int __user *outp;
};

static void ksu_install_fd_tw_func(struct callback_head *cb)
{
    struct ksu_install_fd_tw *tw = container_of(cb, struct ksu_install_fd_tw, cb);
    int fd = ksu_install_fd();
    pr_info("[%d] install ksu fd: %d\n", current->pid, fd);

    if (copy_to_user(tw->outp, &fd, sizeof(fd))) {
        pr_err("install ksu fd reply err\n");
        close_fd(fd);
    }

    kfree(tw);
}

static int ksu_handle_fd_request(void __user *arg)
{
    struct ksu_install_fd_tw *tw;

    tw = kzalloc(sizeof(*tw), GFP_ATOMIC);
    if (!tw)
        return -ENOMEM;

    tw->outp = (int __user *)arg;
    tw->cb.func = ksu_install_fd_tw_func;

    if (task_work_add(current, &tw->cb, TWA_RESUME)) {
        kfree(tw);
        pr_warn("install fd add task_work failed\n");
        return -EINVAL;
    }

    return 0;
}

int ksu_handle_sys_reboot(int magic1, int magic2, unsigned int cmd, void __user **arg)
{
    if (magic1 != KSU_INSTALL_MAGIC1)
        return -EINVAL;

    // Rare case that unlikely to happen
    if (unlikely(!arg))
        return -EINVAL;


// If magic2 is susfs and current process is root
    if (magic2 == SUSFS_MAGIC && current_uid().val == 0) {
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
        if (cmd == CMD_SUSFS_ADD_SUS_PATH) {
            susfs_add_sus_path(arg);
            return 0;
        }
        if (cmd == CMD_SUSFS_ADD_SUS_PATH_LOOP) {
            susfs_add_sus_path_loop(arg);
            return 0;
        }
#endif //#ifdef CONFIG_KSU_SUSFS_SUS_PATH
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
        if (cmd == CMD_SUSFS_HIDE_SUS_MNTS_FOR_ALL_PROCS) {
            susfs_set_hide_sus_mnts_for_all_procs(arg);
            return 0;
        }
#endif //#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
        if (cmd == CMD_SUSFS_ADD_SUS_KSTAT) {
            susfs_add_sus_kstat(arg);
            return 0;
        }
        if (cmd == CMD_SUSFS_UPDATE_SUS_KSTAT) {
            susfs_update_sus_kstat(arg);
            return 0;
        }
        if (cmd == CMD_SUSFS_ADD_SUS_KSTAT_STATICALLY) {
            susfs_add_sus_kstat(arg);
            return 0;
        }
#endif //#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
#ifdef CONFIG_KSU_SUSFS_TRY_UMOUNT
        if (cmd == CMD_SUSFS_ADD_TRY_UMOUNT) {
            susfs_add_try_umount(arg);
            return 0;
        }
#endif //#ifdef CONFIG_KSU_SUSFS_TRY_UMOUNT
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
        if (cmd == CMD_SUSFS_SET_UNAME) {
            susfs_set_uname(arg);
            return 0;
        }
#endif //#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
        if (cmd == CMD_SUSFS_ENABLE_LOG) {
            susfs_enable_log(arg);
            return 0;
        }
#endif //#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
        if (cmd == CMD_SUSFS_SET_CMDLINE_OR_BOOTCONFIG) {
            susfs_set_cmdline_or_bootconfig(arg);
            return 0;
        }
#endif //#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
#ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT
        if (cmd == CMD_SUSFS_ADD_OPEN_REDIRECT) {
            susfs_add_open_redirect(arg);
            return 0;
        }
#endif //#ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT
#ifdef CONFIG_KSU_SUSFS_SUS_MAP
        if (cmd == CMD_SUSFS_ADD_SUS_MAP) {
            susfs_add_sus_map(arg);
            return 0;
        }
#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MAP
        if (cmd == CMD_SUSFS_ENABLE_AVC_LOG_SPOOFING) {
            susfs_set_avc_log_spoofing(arg);
            return 0;
        }
        if (cmd == CMD_SUSFS_SHOW_ENABLED_FEATURES) {
            susfs_get_enabled_features(arg);
            return 0;
        }
        if (cmd == CMD_SUSFS_SHOW_VARIANT) {
            susfs_show_variant(arg);
            return 0;
        }
        if (cmd == CMD_SUSFS_SHOW_VERSION) {
            susfs_show_version(arg);
            return 0;
        }
        return 0;
    }

#ifdef CONFIG_KSU_DEBUG
    pr_info("sys_reboot: magic: 0x%x (id: %d)\n", magic1, magic2);
#endif

    // Dereference **arg.. with IS_ERR check.
    void __user *argp = (void __user *)*arg;
    if (IS_ERR(argp)) {
        pr_err("Failed to deref user arg, err: %lu\n", PTR_ERR(argp));
        return -EINVAL;
    }

    // Check if this is a request to install KSU fd
    if (magic2 == KSU_INSTALL_MAGIC2) {
        return ksu_handle_fd_request(argp);
    }

    return 0;
}

void __init ksu_supercalls_init(void)
{
    ksu_supercall_dump_commands();
}

void __exit ksu_supercalls_exit(void)
{
    ksu_supercall_cleanup_state();
}
