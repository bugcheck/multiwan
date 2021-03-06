// -*- c-basic-offset: 4 -*-
#include <click/config.h>
#include "processmwanheader.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

ProcessMWanHeader::ProcessMWanHeader()
    : _elem_ps(0), _max_paint(0), _bandwidths(0), _schedule(0)
{
}

ProcessMWanHeader::~ProcessMWanHeader()
{
}

int
ProcessMWanHeader::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Element *ps_element = 0;
    if (cp_va_kparse(conf, this, errh,
		     "PROGSCHEDULER", cpkM, cpElement, &ps_element,
		     "MAX_PAINT", cpkM, cpUnsigned, &_max_paint,
		     cpEnd) < 0)
	return -1;

    ProgScheduler *ps = 0;
    if (ps_element && 
        !(ps = (ProgScheduler *)(ps_element->cast("ProgScheduler"))))
	return errh->error("PROGSCHEDULER must be a ProgScheduler element");
    else if (ps)
	_elem_ps = ps;

    _bandwidths = new uint32_t[_max_paint];
    _schedule = new int[_max_paint];

    for (unsigned int i = 0; i < _max_paint; i++) {
        _bandwidths[i] = 0;
        _schedule[i] = 1;
    }

#ifdef CLICK_PROCESSMWANHEADER_DEBUG
    click_chatter("Done with configure");
#endif

    return 0;
}

int
ProcessMWanHeader::initialize(ErrorHandler *)
{
    return 0;
}

void
ProcessMWanHeader::add_handlers()
{
}

void
ProcessMWanHeader::cleanup(CleanupStage)
{
    delete _bandwidths;
    delete _schedule;
}

Packet *
ProcessMWanHeader::simple_action(Packet *p)
{
#ifdef CLICK_PROCESSMWANHEADER_DEBUG
    click_chatter("A packet!");
#endif

    //TODO: This is wrong and a hack! Make this portable.
    int paint = PAINT_ANNO(p);
    uint32_t bw = *((uint32_t*) (p->data()+8));

#ifdef CLICK_PROCESSMWANHEADER_DEBUG
    click_chatter("It says the bandwidth is %d", bw);
#endif

    _bandwidths[paint] = bw;

    if (update_schedule(_max_paint, _bandwidths)) {
        _elem_ps->set_schedule(_max_paint, _schedule);
#ifdef CLICK_PROCESSMWANHEADER_DEBUG
	click_chatter("The schedule has changed!");
#endif
    }

    return p;
}

/*
  Returns true if schedule changes.

  Takes the largest bandwidth value and makes that schedule value
  MAX_SCH_VALUE. Then divides the largest value with with the max to get the
  divider to find the proper proportions for all the others.
 */
bool
ProcessMWanHeader::update_schedule(int size, uint32_t *bandwidths)
{
#ifdef CLICK_PROCESSMWANHEADER_DEBUG
    click_chatter("Updating schedule");
#endif
    int tmp_sch[size];

    uint32_t max = 0;
    int max_paint = 0;

    for (int i = 0; i < size; i++) {
#ifdef CLICK_PROCESSMWANHEADER_DEBUG
	click_chatter("bandwidth[%d] = %d", i, bandwidths[i]);
#endif
        if (max < bandwidths[i]) {
            max = bandwidths[i];
            max_paint = i;
        }
    }

#ifdef CLICK_PROCESSMWANHEADER_DEBUG
    click_chatter("Max bandwidth is %d", max);
#endif

    int divisor = max/MAX_SCH_VALUE;

#ifdef CLICK_PROCESSMWANHEADER_DEBUG
    click_chatter("Divisor is %d", divisor);
#endif

    for (int i = 0; i < size; i++) {
        int value = bandwidths[i]/divisor;
        tmp_sch[i] = value > 0 ? value : MIN_SCH_VALUE;
    }

    if (!static_ary_equals(size, tmp_sch, _schedule)) {
        for (int i = 0; i < size; i++)
            _schedule[i] = tmp_sch[i];

        return true;
    }

    return false;
}

bool
ProcessMWanHeader::static_ary_equals(int size, int *a, int *b)
{
    for (int i = 0; i < size; i++)
        if (a[i] != b[i])
            return false;

    return true;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ProcessMWanHeader)
