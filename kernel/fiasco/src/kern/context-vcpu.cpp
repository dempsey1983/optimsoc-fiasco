INTERFACE:

#include "vcpu.h"

EXTENSION class Context
{
protected:
  Ku_mem_ptr<Vcpu_state> _vcpu_state;
};


IMPLEMENTATION:

PUBLIC inline
Context::Ku_mem_ptr<Vcpu_state> const &
Context::vcpu_state() const
{ return _vcpu_state; }


PUBLIC inline
Mword
Context::vcpu_disable_irqs()
{
  if (EXPECT_FALSE(state() & Thread_vcpu_enabled))
    {
      Vcpu_state *vcpu = vcpu_state().access();
      Mword s = vcpu->state;
      vcpu->state = s & ~Vcpu_state::F_irqs;
      return s & Vcpu_state::F_irqs;
    }
  return 0;
}

PUBLIC inline
void
Context::vcpu_restore_irqs(Mword irqs)
{
  if (EXPECT_FALSE((irqs & Vcpu_state::F_irqs)
                   && (state() & Thread_vcpu_enabled)))
    vcpu_state().access()->state |= Vcpu_state::F_irqs;
}

PUBLIC inline
void
Context::vcpu_save_state_and_upcall()
{
  extern char leave_by_vcpu_upcall[];
  _exc_cont.activate(regs(), leave_by_vcpu_upcall);
}

PUBLIC inline NEEDS["fpu.h", "space.h", Context::arch_load_vcpu_kern_state]
bool
Context::vcpu_enter_kernel_mode(Vcpu_state *vcpu)
{
  if (EXPECT_FALSE(state() & Thread_vcpu_enabled))
    {
      state_del_dirty(Thread_vcpu_user);
      vcpu->_saved_state = vcpu->state;
      Mword flags = Vcpu_state::F_traps
	            | Vcpu_state::F_user_mode;
      vcpu->state &= ~flags;

      if (vcpu->_saved_state & Vcpu_state::F_user_mode)
	vcpu->_sp = vcpu->_entry_sp;
      else
	vcpu->_sp = regs()->sp();

      if (_space.user_mode())
	{
	  _space.user_mode(false);
	  state_del_dirty(Thread_vcpu_fpu_disabled);

          bool load_cpu_state = current() == this;

          arch_load_vcpu_kern_state(vcpu, load_cpu_state);

	  if (load_cpu_state)
	    {
	      if (state() & Thread_fpu_owner)
		Fpu::enable();

	      space()->switchin_context(vcpu_user_space());
	      return true;
	    }
	}
    }
  return false;
}



PUBLIC inline
bool
Context::vcpu_irqs_enabled(Vcpu_state *vcpu) const
{
  return EXPECT_FALSE(state() & Thread_vcpu_enabled)
    && vcpu->state & Vcpu_state::F_irqs;
}

PUBLIC inline
bool
Context::vcpu_pagefaults_enabled(Vcpu_state *vcpu) const
{
  return EXPECT_FALSE(state() & Thread_vcpu_enabled)
    && vcpu->state & Vcpu_state::F_page_faults;
}

PUBLIC inline
bool
Context::vcpu_exceptions_enabled(Vcpu_state *vcpu) const
{
  return EXPECT_FALSE(state() & Thread_vcpu_enabled)
    && vcpu->state & Vcpu_state::F_exceptions;
}

PUBLIC inline
void
Context::vcpu_set_irq_pending()
{
  if (EXPECT_FALSE(state() & Thread_vcpu_enabled))
    vcpu_state().access()->sticky_flags |= Vcpu_state::Sf_irq_pending;
}

/** Return the space context.
    @return space context used for this execution context.
            Set with set_space_context().
 */
PUBLIC inline
Space *
Context::vcpu_user_space() const
{ return _space.vcpu_user(); }


// --------------------------------------------------------------------------
INTERFACE [debug]:

EXTENSION class Context
{
  static unsigned vcpu_log_fmt(Tb_entry *, int, char *)
  asm ("__context_vcpu_log_fmt");
};


// --------------------------------------------------------------------------
IMPLEMENTATION [debug]:

#include "kobject_dbg.h"
#include "string_buffer.h"

IMPLEMENT
void
Context::Vcpu_log::print(String_buffer *buf) const
{
  switch (type)
    {
    case 0:
    case 4:
      buf->printf("%sret pc=%lx sp=%lx state=%lx task=D:%lx",
                  type == 4 ? "f" : "", ip, sp, state, space);
      break;
    case 1:
      buf->printf("ipc from D:%lx task=D:%lx sp=%lx",
                  Kobject_dbg::pointer_to_id((Kobject*)ip), state, sp);
      break;
    case 2:
      buf->printf("exc #%x err=%lx pc=%lx sp=%lx state=%lx task=D:%lx",
                  (unsigned)trap, err, ip, sp, state, space);
      break;
    case 3:
      buf->printf("pf  pc=%lx pfa=%lx state=%lx task=D:%lx", ip, sp, state, space);
      break;
    default:
      buf->printf("vcpu: unknown");
      break;
    }
}

