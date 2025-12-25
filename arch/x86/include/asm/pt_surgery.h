/* for make m log and ds log */
extern int make_pgd_m_log(pgd_t *pgd);
extern int make_pud_m_log(p4d_t *p4d, pud_t *pud);
extern int make_pmd_m_log(pud_t *pud, pmd_t *pmd);
extern int make_pte_m_log(pmd_t *pmd, pte_t *pte);

/* for destroy m log and ds log */
extern void destroy_pgd_m_log(struct page *pgd_page);
extern void destroy_pud_m_log(struct page *pud_page);
extern void destroy_pmd_m_log(struct page *pmd_page);
extern void destroy_pte_m_log(struct page *pte_page);

/* for synchronization between pte and ds log */
extern int make_pte_ds_log_usr(pte_t *ptep, pte_t pte);
extern int make_pte_ds_log_usr_pid(pte_t *ptep, pte_t pte, pid_t pid);
extern int clear_wrbit_ds_log(pte_t *ptep);

/* for continuous pte read/write */
extern int check_pte_is_broken_for_pte_write(pte_t *ptep);
extern pte_t check_pte_is_broken_for_pte_read(pte_t *ptep);

/* for reference counter */
extern void inc_page_ref_count_read(pte_t *ptep);
extern void inc_page_ref_count_write(pte_t *ptep);
extern void dec_page_ref_count(pte_t *ptep);

/* for fork() */
extern bool is_parent_valid_pt_surgery(pid_t ppid, pid_t pid);
extern void pt_surgery_register_child(struct task_struct *p);
