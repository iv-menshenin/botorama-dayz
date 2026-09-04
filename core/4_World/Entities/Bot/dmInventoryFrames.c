class dmInventoryFrames
{
    dmAISurvivorBase m_Pawn;

    void dmInventoryFrames(dmAISurvivorBase pawn)
    {
        m_Pawn = pawn;
    }

    
}

enum dmInventoryDoing {
    PLACEONGROUND,
    ATTACHTOSLOT,
    TAKEINTOCARGO,
    PUTINTOHANDS
}

class dmInventoryFrame
{
    ref dmInventoryFrame m_OnSuccess;
    ref dmInventoryFrame m_OnFail;

    dmInventoryDoing m_ToDo;

    static dmInventoryFrame SuccessFail(dmInventoryFrame s, dmInventoryFrame f)
    {
        return new dmInventoryFrame(s, f);
    }

    static dmInventoryFrame SuccessOnly(dmInventoryFrame s)
    {
        return new dmInventoryFrame(s, null);
    }

    static dmInventoryFrame FailOnly(dmInventoryFrame f)
    {
        return new dmInventoryFrame(null, f);
    }

    static dmInventoryFrame EvenFail(dmInventoryFrame a)
    {
        return new dmInventoryFrame(a, a);
    }

    void dmInventoryFrame(dmInventoryFrame s, dmInventoryFrame f)
    {
        m_OnSuccess = s;
        m_OnFail = f;
    }

    dmInventoryFrame DoNext()
    {
        if ( ExecuteCurrent() )
        {
            return m_OnSuccess;
        }
        return m_OnFail;
    }

    bool ExecuteCurrent()
    {
        switch( m_ToDo )
        {
        case dmInventoryDoing.PLACEONGROUND:
            break;
        case dmInventoryDoing.ATTACHTOSLOT:
            break;
        case dmInventoryDoing.TAKEINTOCARGO:
            break;
        case dmInventoryDoing.PUTINTOHANDS:
            break;
        }
        return false;
    }
}