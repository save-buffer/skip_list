#pragma once
#include <cstdint>

struct MwcasDescriptor;
struct WordDescriptor;

constexpr uint64_t RDCSSFlag = 1ULL << 63;
constexpr uint64_t MwcasFlag = 1ULL << 62;
constexpr uint64_t AddressMask = (1ULL << 60) - 1;

enum MwcasStatus
{
    Undecided,
    Succeeded,
    Failed,
};

struct WordDescriptor
{
    uint64_t *addr;
    uint64_t old_val;
    uint64_t new_val;
    MwcasDescriptor *desc;
};

struct MwcasDescriptor
{
    MwcasStatus status;
    uint64_t count;
    WordDescriptor words[256];

    MwcasDescriptor() : status(Undecided), count(0) {}
    
    void add_word(uint64_t *addr, uint64_t old_val, uint64_t new_val)
    {
	words[count++] = { addr, old_val, new_val, this };
    }
};

void complete_install(WordDescriptor *wd)
{
    uint64_t ptr = (uint64_t)(wd->desc) | MwcasFlag;
    bool u = wd->desc->status == Undecided;
    __sync_bool_compare_and_swap(wd->addr, (uint64_t)(wd) | RDCSSFlag, u ? ptr : wd->old_val);
}

uint64_t install_mwcas_descriptor(WordDescriptor *wd)
{
    uint64_t ptr = (uint64_t)(wd) | RDCSSFlag;
retry:    
    uint64_t val = __sync_val_compare_and_swap(wd->addr, wd->old_val, ptr);
    if(val & RDCSSFlag)
    {
	complete_install((WordDescriptor *)(val & AddressMask));
	goto retry;
    }

    if(val == wd->old_val)
	complete_install(wd);
    return val;
}

bool mwcas(MwcasDescriptor *md)
{
    MwcasStatus st = Succeeded;
    // md.words should be sorted in order of address
    for(int i = 0; i < md->count; i++)	
    {
	auto &w = md->words[i];
    retry:
	uint64_t rval = install_mwcas_descriptor(&w);
	if(rval == w.old_val)
	    continue;
	else if(rval & MwcasFlag)
	{
	    mwcas(reinterpret_cast<MwcasDescriptor *>(reinterpret_cast<WordDescriptor *>(rval)->addr));
	    goto retry;
	}
	else
	{
	    st = Failed;
	    break;
	}
    }

    __sync_val_compare_and_swap(&md->status, Undecided, st);
    for(int i = 0; i < md->count; i++)
    {
	auto &w = md->words[i];
	uint64_t v = md->status == Succeeded ? w.new_val : w.old_val;
	uint64_t expected = (uint64_t)(md) | MwcasFlag;
	uint64_t rval = __sync_val_compare_and_swap(w.addr, expected, v);
	if(rval == (uint64_t)(md) | MwcasFlag)
	    __sync_bool_compare_and_swap(w.addr, expected, v);	
    }
    return md->status == Succeeded;
}
