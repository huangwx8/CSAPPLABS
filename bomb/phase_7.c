int fun7(int* p, int x) {
    if (!p) return -1;
    int y = p->val;
    if (x<y) {
        return 2*fun7(p->left, x);
    }
    else if (x==y){
        return 0;
    }
    else {
        return 2*fun7(p->right, x)+1;
    }
}

void phase_7() {
    char str[100];
    scanf("%s",s);
    int x = strtol(str,0,10);
    if ((unsigned)(x-1)>0x3e8) explode_bomb();
    int t = fun7(0x804b200, x);
    if (t!=6) explode_bomb();
}