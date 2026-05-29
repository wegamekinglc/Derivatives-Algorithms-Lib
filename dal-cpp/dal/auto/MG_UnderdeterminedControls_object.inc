

UnderdeterminedControls_::UnderdeterminedControls_(const Dictionary_& src)
        :
maxEvaluations_(Dictionary::Extract(src, "MAXEVALUATIONS", [](const Cell_& c) { return Cell::ToInt(c);})), 
maxRestarts_(Dictionary::Extract(src, "MAXRESTARTS", [](const Cell_& c) { return Cell::ToInt(c);})), 
maxBacktrackTries_(Dictionary::Extract(src, "MAXBACKTRACKTRIES", [](const Cell_& c) { return Cell::ToInt(c);}, 5)), 
restartTolerance_(Dictionary::Extract(src, "RESTARTTOLERANCE", [](const Cell_& c) { return Cell::ToDouble(c);}, 0.4)), 
backtrackTolerance_(Dictionary::Extract(src, "BACKTRACKTOLERANCE", [](const Cell_& c) { return Cell::ToDouble(c);}, 0.1)), 
maxBacktrack_(Dictionary::Extract(src, "MAXBACKTRACK", [](const Cell_& c) { return Cell::ToDouble(c);}, 0.8))
{
        REQUIRE(maxEvaluations_ > 0, "Validation failed: " "maxEvaluations_ > 0");
        REQUIRE(maxRestarts_ > 0, "Validation failed: " "maxRestarts_ > 0");
        REQUIRE(restartTolerance_ >= 0.0 && restartTolerance_ <= 1.0, "Validation failed: " "restartTolerance_ >= 0.0 && restartTolerance_ <= 1.0");
        REQUIRE(maxBacktrack_ > backtrackTolerance_ && maxBacktrack_ < 1.0, "Validation failed: " "maxBacktrack_ > backtrackTolerance_ && maxBacktrack_ < 1.0");	
}
Dictionary_ UnderdeterminedControls_::Data() const
{
        Dictionary_ retval;        
        retval.Insert("MAXEVALUATIONS", Cell::FromInt(maxEvaluations_));
        retval.Insert("MAXRESTARTS", Cell::FromInt(maxRestarts_));
        retval.Insert("MAXBACKTRACKTRIES", Cell::FromInt(maxBacktrackTries_));
        retval.Insert("RESTARTTOLERANCE", Cell::FromDouble(restartTolerance_));
        retval.Insert("BACKTRACKTOLERANCE", Cell::FromDouble(backtrackTolerance_));
        retval.Insert("MAXBACKTRACK", Cell::FromDouble(maxBacktrack_));
        return retval;
}
