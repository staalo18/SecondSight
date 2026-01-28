scriptname _ts_SecondSightEffectScript extends ActiveMagicEffect

int stage = 0 ; 0: not active, 1: intro, 2: main

ImageSpaceModifier property Imod auto
ImageSpaceModifier property ImodIntro auto
ImageSpaceModifier property ImodOutro auto


event OnEffectStart(actor akTarget, actor akCaster)

    actor targetActor = _ts_SecondSightFunctions.GetCrosshairTarget(maxTargetDistance = 8000.0)

    if !targetActor
        return
    endif

	if stage == 0
		stage = 1
		ImodIntro.apply()
        _ts_SecondSightFunctions.StartSecondSightEffect(targetActor)
		utility.wait(1.0) ; 1 sec until IModIntro has progressed to match Imod settings
		if stage == 1
			ImodIntro.PopTo(Imod)	
			stage = 2
		endif
	elseif stage == 1
		ImodIntro.PopTo(Imod)	
		stage = 2
		self.dispel()
	else
		self.dispel()
	endif


endEvent

event OnEffectFinish(actor akTarget, actor akCaster)

    _ts_SecondSightFunctions.StopSecondSightEffect()

    if stage == 1
        ImodIntro.PopTo(ImodOutro)
    elseif stage == 2
        Imod.PopTo(ImodOutro)
    endif
    ImodIntro.remove()
    Imod.remove()
    stage = 0
endEvent
