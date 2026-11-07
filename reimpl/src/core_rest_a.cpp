// Small functions of 0x400000-0x47ffff left over by the earlier batches (phase 3 remains, band A;
// docs/DECOMPILATION.md section 8, "Phase 3 remains, band A"): three shared constant slots, the flags field
// kind's precache slot, the binary tree helpers of the handle heap, CQuestStatColumnInfo's and
// CFlyweightLayerPartAI's deleting destructors.
//
//   0x401280  slot 27 of CInventoryItem and 24 more vtables: eax 0 (three stack arguments)
//   0x40c0a0  slot 27 of CAction and 90 more vtables: al 0 (four stack arguments)
//   0x46cd80  slot 20 (HasValue) of every field kind but the dynamic properties, and other vtables' slots:
//             al 1 (one stack argument)
//   0x4150c0  slot 32 (precache) of the flags field kind (vtable 0x5ec684): with +0x34 set, for every flag
//             set in the object's value (the dword at +0x10, read again for every flag) slot 12 of the
//             name list +0x2c with the flag's number; the count (the list's slot 22) is asked again after
//             every flag. CObject's slot 7 (0x4167b0) calls slot 32 of every field.
//   0x431950-0x431a70  the handle heap's tree of free blocks (callers HandleHeap 0x4d2660-0x4d2bc0; nodes
//             {key, parent, right (key >= node), left (key < node)}): the smallest node with a key not
//             below k (0x431950), insert below a root (0x431990), unlink (0x4319e0: a node with two
//             children is replaced by its left child, the right subtree hung below the left subtree's
//             rightmost node), unlink and insert again with a new key (0x431a70).
//   0x426bf0  CQuestStatColumnInfo's deleting destructor (0x28 bytes, vtable 0x5ee174, four ZStrings
//             +8 to +0x14 destroyed from the last; the array form from the last element).
//   0x467140  CFlyweightLayerPartAI's deleting destructor (slot 1 of 0x5ef1a0; no array form, pool 0x10).
//
// Return values: what the original leaves in eax where a caller could see it; 0x4319e0's eax (the last
// value it moved through eax) is reproduced too.
#include "class_info.h"
#include "ai_common.h"

namespace zax {

using namespace ai;

namespace {

// ---- Shared constant slots ---------------------------------------------------------------------------------

DWORD THISCALL Zero3(void *, DWORD, DWORD, DWORD) { return 0; }
BYTE THISCALL False4(void *, DWORD, DWORD, DWORD, DWORD) { return 0; }
BYTE THISCALL True1(void *, DWORD) { return 1; }

// ---- The flags field kind's precache --------------------------------------------------------------------

// 0x4150c0 (slot 32: object). eax: the last count (void in use [assumption]).
DWORD THISCALL FlagsField_Precache(BYTE *self, BYTE *object)
{
    if (!fl<BYTE>(self, 0x34))
        return 0;
    DWORD i = 0;
    DWORD n = vcall<DWORD>(bp(dw(self, 0x2c)), 22);
    if (!n)
        return n;
    do {
        if (dw(object + dw(self, 0x10), 0) & (1u << (i & 31)))
            vcall<DWORD>(bp(dw(self, 0x2c)), 12, i);
        i++;
        n = vcall<DWORD>(bp(dw(self, 0x2c)), 22);
    } while (i < n);
    return n;
}

// ---- The handle heap's tree ---------------------------------------------------------------------------------

struct Node {
    DWORD key;
    Node *parent, *right, *left;
};

// 0x431950 (thiscall on the root, may be null: key): the node with that key, else the smallest with a
// bigger one (null when there is none)
Node *THISCALL Tree_LowerBound(Node *n, DWORD key)
{
    Node *best = nullptr;
    if (!n)
        return n;
    for (;;) {
        if (key == n->key)
            return n;
        if (key < n->key) {
            if (!n->left)
                return n;
            best = n;
            n = n->left;
        } else {
            n = n->right;
            if (!n)
                return best;
        }
    }
}

// 0x431990 (thiscall on the new node: root, key). eax: the new node's parent.
Node *THISCALL Tree_Insert(Node *self, Node *at_, DWORD key)
{
    self->right = nullptr;
    self->left = nullptr;
    self->key = key;
    for (;;) {
        if (key < at_->key) {
            if (!at_->left) {
                at_->left = self;
                self->parent = at_;
                return at_;
            }
            at_ = at_->left;
        } else {
            if (!at_->right) {
                at_->right = self;
                self->parent = at_;
                return at_;
            }
            at_ = at_->right;
        }
    }
}

// 0x4319e0 (thiscall on the node; it has a parent). eax: the value the original's last move through eax
// left (the parent, the left child or the right subtree's parent link).
DWORD THISCALL Tree_Unlink(Node *self)
{
    Node *p = self->parent;
    Node **link = p->right == self ? &p->right : &p->left;
    DWORD eax = (DWORD)(uintptr_t)p;
    Node *r = self->right;
    if (!r) {
        Node *l = self->left;
        eax = (DWORD)(uintptr_t)l;
        if (!l) {
            *link = r;
        } else {
            *link = l;
            eax = (DWORD)(uintptr_t)self->parent;
            self->left->parent = self->parent;
        }
    } else {
        Node *l = self->left;
        eax = (DWORD)(uintptr_t)l;
        if (!l) {
            *link = r;
            eax = (DWORD)(uintptr_t)self->parent;
            self->right->parent = self->parent;
        } else {
            *link = l;
            self->left->parent = self->parent;
            Node *m = self->left;
            Node *k = m->right;
            while (k) {
                m = k;
                k = m->right;
            }
            m->right = self->right;
            eax = (DWORD)(uintptr_t)self->right;
            self->right->parent = m;
        }
    }
    self->right = nullptr;
    self->left = nullptr;
    self->parent = nullptr;
    return eax;
}

// 0x431a70 (thiscall on the node: root, new key): unlinked and inserted again. eax: the new parent.
Node *THISCALL Tree_Move(Node *self, Node *root, DWORD key)
{
    fn<DWORD(THISCALL *)(Node *)>(0x4319e0)(self);
    self->right = nullptr;
    self->left = nullptr;
    self->key = key;
    for (;;) {
        if (key < root->key) {
            if (!root->left) {
                root->left = self;
                self->parent = root;
                return root;
            }
            root = root->left;
        } else {
            if (!root->right) {
                root->right = self;
                self->parent = root;
                return root;
            }
            root = root->right;
        }
    }
}

// ---- Deleting destructors ------------------------------------------------------------------------------

void column_info_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5ee174));
    zs_dtor(o + 0x14);
    zs_dtor(o + 0x10);
    zs_dtor(o + 0xc);
    zs_dtor(o + 8);
    obj_dtor(o);
}
// 0x426bf0 (slot 1 of 0x5ee174)
void *THISCALL ColumnInfo_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x28, column_info_destroy>(self, flags); }

// 0x467140 (slot 1 of 0x5ef1a0): the single object only
BYTE *THISCALL FlyweightAI_DeletingDtor(BYTE *self, DWORD flags)
{
    vtable_and_free(self, vt(0x5ef1a0));
    vtable_and_free(self, vt(0x5ed1d0));
    ai_base_dtor(self);
    if (flags & 1)
        pool_free(self, 0x10);
    return self;
}

}  // namespace

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(core_rest_a)
{
    const Hook list[] = {
        {0x401280, (void *)&Zero3, "Slot_Zero3", 5, {0x33, 0xc0, 0xc2, 0x0c, 0x00, 0x90, 0x90, 0x90}},
        {0x40c0a0, (void *)&False4, "Slot_False4", 5, {0x32, 0xc0, 0xc2, 0x10, 0x00, 0x90, 0x90, 0x90}},
        {0x46cd80, (void *)&True1, "Stub_ReturnTrue4", 5, {0xb0, 0x01, 0xc2, 0x04, 0x00, 0x90, 0x90, 0x90}},
        {0x4150c0, (void *)&FlagsField_Precache, "FlagsField_Precache", 7, {0x56, 0x57, 0x8b, 0xf9, 0x8a, 0x47, 0x34, 0x84}},
        {0x426bf0, (void *)&ColumnInfo_DeletingDtor, "CQuestStatColumnInfo::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x57}},
        {0x431950, (void *)&Tree_LowerBound, "HandleTree_LowerBound", 6, {0x8b, 0xc1, 0x33, 0xd2, 0x85, 0xc0, 0x56, 0x74}},
        {0x431990, (void *)&Tree_Insert, "HandleTree_Insert", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x8b, 0x74, 0x24}},
        {0x4319e0, (void *)&Tree_Unlink, "HandleTree_Unlink", 7, {0x8b, 0x41, 0x04, 0x56, 0x8b, 0x70, 0x08, 0x8d}},
        {0x431a70, (void *)&Tree_Move, "HandleTree_Move", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x68, 0xff, 0xff, 0xff}, 4},
        {0x467140, (void *)&FlyweightAI_DeletingDtor, "CFlyweightLayerPartAI::DeletingDtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0xa0, 0xf1, 0x5e}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
