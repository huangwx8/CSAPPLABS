extern node* p; // 链表头，程序中的0x804b2b4

void phase_6() {
    char str[100];
    scanf("%s",s);
    int val = strtol(str,0,10);
    p->val = val;
    node* head = fun6(p); // 排序
    head = head->next;
    head = head->next;
    head = head->next;
    head = head->next;
    head = head->next;
    if (head->next!=p) explode_bomb();
}