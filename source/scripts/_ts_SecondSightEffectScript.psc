scriptname _ts_SecondSightEffectScript extends ActiveMagicEffect

int stage = 0 ; 0: not active, 1: intro, 2: main
int soundInstanceID = 0

ImageSpaceModifier property Imod auto
ImageSpaceModifier property ImodIntro auto
ImageSpaceModifier property ImodOutro auto
sound property SoundFXIntro auto
sound property SoundFXOutro auto
sound property SoundFXLoop auto


event OnEffectStart(actor akTarget, actor akCaster)

	if stage == 0
        if (!_ts_SecondSightFunctions.StartSecondSightEffect())
			return
		endif
		
		stage = 1
		SoundFXIntro.play((akTarget as ObjectReference))
		ImodIntro.apply()
		utility.wait(1.0) ; 1 sec until IModIntro has progressed to match Imod settings
		if stage == 1
			ImodIntro.PopTo(Imod)	
			soundInstanceID = SoundFXLoop.play((akTarget as ObjectReference))
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
		SoundFXOutro.play((akTarget as ObjectReference))
    elseif stage == 2
        Imod.PopTo(ImodOutro)
		SoundFXOutro.play((akTarget as ObjectReference))
		utility.wait(0.1)
		Sound.StopInstance(soundInstanceID)
    endif
    ImodIntro.remove()
    Imod.remove()
    stage = 0
endEvent
