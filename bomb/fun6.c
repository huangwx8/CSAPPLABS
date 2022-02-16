node* fun6(node* p) {
    /*
    p: %eax
    p1: %esi
    p1_val: %ebx
    p2: %ecx
    p3: %edx
    */
    node* p1 = p->next, p2, p3;
    p->next = 0;
    int p1_val;
    if (!p1) return p;
    go to entry;
    while (1) {
        while (1) {
            p2 = p1->next;
            p1->next = p3;
            if (!p2) return p;
            p1 = p2;
        entry:
            if (!p) {
                p3 = p;
                p = p1;
            }
            else {
                p1_val = p1->val;
                p2 = p;
                if ((p->val)>p1_val) break;
                p3 = p;
                p = p1;
            }
        }
        do {
            p3 = p2->next;
            if (p3 && (p3->val)>(p1_val)) {
                p2 = p3;
                continue;
            }
            else break;
        } while (1);
        if (p2==p3) {
            p = p1;
        }
        else {
            p2->next = p1;
        }
    }
}


node* fun6(node* L1) {
    node* L2 = L1->next;
    L1->next = 0;
    while (L2) {
        // 把L2的降序部分尽可能放到L1的首部
        while (L2 && L2->val>=L1->val) {
            node* tmp = L2->next;
            L2->next = L1;
            L1 = L2;
            L2 = tmp;
        }
        // 把L2的首部结点插入到合适的位置
        node *p = L2;
        L2 = L2->next;
        node* hole = L1, *hole_next = L1->next;
        while (hole_next && hole_next->val>p->val) {
            hole = hole_next;
            hole_next = hole->next;
        }
        hole->next = p;
        p->next = hole_next;
    }
    return L1;
}