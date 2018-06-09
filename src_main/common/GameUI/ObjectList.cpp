// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "ObjectList.h"

#include <malloc.h>
#include <cstdio>

#include "tier1/strtools.h"

#include "tier0/include/memdbgon.h"

ObjectList::ObjectList() {
  head = tail = current = nullptr;
  number = 0;
}

ObjectList::~ObjectList() { Clear(false); }

bool ObjectList::AddHead(void *newObject) {
  // create new element
  element_t *newElement = (element_t *)calloc(1, sizeof(element_t));

  // out of memory
  if (newElement == nullptr) return false;

  // insert element
  newElement->object = newObject;

  if (head) {
    newElement->next = head;
    head->prev = newElement;
  };

  head = newElement;

  // if list was empty set new tail
  if (tail == nullptr) tail = head;

  number++;

  return true;
}

void *ObjectList::RemoveHead() {
  // check head is present
  if (head) {
    void *retObj = head->object;
    element_t *newHead = head->next;
    if (newHead) newHead->prev = nullptr;

    // if only one element is in list also update tail
    // if we remove this prev element
    if (tail == head) tail = nullptr;

    heap_free(head);
    head = newHead;

    number--;

    return retObj;
  }

  return nullptr;
}

bool ObjectList::AddTail(void *newObject) {
  element_t *newElement = (element_t *)calloc(1, sizeof(element_t));
  // out of memory;
  if (newElement == nullptr) return false;

  newElement->object = newObject;

  if (tail) {
    newElement->prev = tail;
    tail->next = newElement;
  }

  tail = newElement;

  // if list was empty set new head
  if (head == nullptr) head = tail;

  number++;

  return true;
}

void *ObjectList::RemoveTail() {
  // check tail is present
  if (tail) {
    void *retObj = tail->object;
    element_t *newTail = tail->prev;
    if (newTail) newTail->next = nullptr;

    // if only one element is in list also update tail
    // if we remove this prev element
    if (head == tail) head = nullptr;

    heap_free(tail);
    tail = newTail;

    number--;

    return retObj;
  }

  return nullptr;
}

bool ObjectList::IsEmpty() { return head == nullptr; }

int ObjectList::CountElements() { return number; }

bool ObjectList::Contains(void *object) {
  element_t *e = head;

  while (e && e->object != object) {
    e = e->next;
  }

  if (e) {
    current = e;
    return true;
  } else {
    return false;
  }
}

void ObjectList::Clear(bool freeElementsMemory) {
  element_t *ne;

  element_t *e = head;
  while (e) {
    ne = e->next;

    if (freeElementsMemory) heap_free(e->object);

    heap_free(e);
    e = ne;
  }

  head = tail = current = nullptr;
  number = 0;
}

bool ObjectList::Remove(void *object) {
  element_t *e = head;

  while (e && e->object != object) {
    e = e->next;
  }

  if (e != nullptr) {
    if (e->prev) e->prev->next = e->next;
    if (e->next) e->next->prev = e->prev;
    if (head == e) head = e->next;
    if (tail == e) tail = e->prev;
    if (current == e) current = e->next;
    heap_free(e);
    number--;
  }

  return e != nullptr;
}

void ObjectList::Init() {
  head = tail = current = nullptr;
  number = 0;
}

void *ObjectList::GetFirst() {
  if (head) {
    current = head->next;
    return head->object;
  }

  current = nullptr;
  return nullptr;
}

void *ObjectList::GetNext() {
  if (current) {
    void *retObj = current->object;
    current = current->next;
    return retObj;
  }

  return nullptr;
}

bool ObjectList::Add(void *newObject) { return AddTail(newObject); }
