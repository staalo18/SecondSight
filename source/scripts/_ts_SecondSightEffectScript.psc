scriptname _ts_SecondSightEffectScript extends ActiveMagicEffect

int stage = 0 ; 0: not active, 1: intro, 2: main
int soundInstanceID = 0
bool isStarting = false ; Guard flag to prevent race conditions

ImageSpaceModifier property Imod auto
ImageSpaceModifier property ImodIntro auto
ImageSpaceModifier property ImodOutro auto
sound property SoundFXIntro auto
sound property SoundFXOutro auto
sound property SoundFXLoop auto


event OnEffectStart(actor akTarget, actor akCaster)
	actor secondSightTarget = _ts_SecondSightFunctions.GetTarget()

; For debug / testing purposes, toggle display of local coordinate frame of speficic body parts:
;	FCFW_SKSEFunctions.ToggleBodyPartRotationMatrixDisplay(target, 1) ; 1: head, 2: torso
;	return;
; ---- End Debug / Testing code ----

	if stage == 0 && !isStarting
		isStarting = true
        if (!_ts_SecondSightFunctions.StartSecondSightEffect())
			isStarting = false
			return
		endif
		
		stage = 1
		SoundFXIntro.play((akCaster as ObjectReference))
		soundInstanceID = SoundFXLoop.play((secondSightTarget as ObjectReference))
		ImodIntro.apply()
		utility.wait(1.0) ; 1 sec until IModIntro has progressed to match Imod settings
		
		; Check stage again after wait - effect might have been stopped during the wait
		if stage == 1 && isStarting
			ImodIntro.PopTo(Imod)	
			stage = 2
		endif

		isStarting = false
	elseif stage == 1 && !isStarting
		ImodIntro.PopTo(Imod)	
		stage = 2
		self.dispel()
	else
		self.dispel()
	endif

endEvent

event OnEffectFinish(actor akTarget, actor akCaster)

; For debug / testing purposes, toggle display of local coordinate frame of speficic body parts:
;	FCFW_SKSEFunctions.ToggleBodyPartRotationMatrixDisplay(target, 1)
;	return;
; ---- End Debug / Testing code ----

	; Mark that we're stopping to prevent any pending OnEffectStart operations
	isStarting = false
	
    _ts_SecondSightFunctions.StopSecondSightEffect()

    if stage == 1
		stage = 0
        ImodIntro.PopTo(ImodOutro)
		SoundFXOutro.play((akCaster as ObjectReference))
    elseif stage == 2
		stage = 0
        Imod.PopTo(ImodOutro)
		utility.wait(0.1)
		SoundFXOutro.play((akCaster as ObjectReference))
    endif
    
	; Clean up
    ImodIntro.remove()
    Imod.remove()

	if soundInstanceID > 0
		utility.wait(0.5)
		Sound.StopInstance(soundInstanceID)
		soundInstanceID = 0
	endif

	stage = 0
endEvent
