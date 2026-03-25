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

/* for safe pte read/write/wrprotect */
extern void ensure_pte_wrprotect_safe(pte_t *ptep);
extern void ensure_pte_write_safe(pte_t *ptep, pte_t pte);
extern pte_t ensure_pte_read_safe(pte_t *ptep);

extern void block_pt_acquire_under_recovery(pte_t *ptep);

/* for reference counter */
extern void pt_page_ref_inc(pte_t *ptep);
extern void pt_page_ref_dec(pte_t *ptep);

/* for fork() */
extern void pt_surgery_register_child(pid_t ppid, pid_t tgid, pid_t pid, struct task_struct *p);
