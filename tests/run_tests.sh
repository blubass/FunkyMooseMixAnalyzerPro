#!/bin/bash
# Funky Moose Test Runner - Robust Edition

API="http://127.0.0.1:5050"
FILES=("tests/test_files/loud_master.wav" "tests/test_files/dynamic_track.wav" "tests/test_files/problematic_bass.wav")
GENRES=("Pop" "Pop" "Techno")

echo "--- Funky Moose Test Runner ---"

# Check for files first
for FILE in "${FILES[@]}"; do
    if [ ! -f "$FILE" ]; then
        echo "Missing test file: $FILE"
        echo "Please put your own test WAV files into tests/test_files/ using these names."
        exit 1
    fi
done

# Run tests
for i in "${!FILES[@]}"; do
    FILE="${FILES[$i]}"
    GENRE="${GENRES[$i]}"
    echo "Testing $FILE ($GENRE)..."
    
    # Upload
    UPLOAD_RES=$(curl -s -X POST -F "audio=@$FILE" -F "genre=$GENRE" "$API/upload")
    REQ_ID=$(echo $UPLOAD_RES | python3 -c "import sys, json; print(json.load(sys.stdin).get('req_id', ''))")
    
    if [ -z "$REQ_ID" ]; then
        echo "Upload failed for $FILE: $UPLOAD_RES"
        continue
    fi
    
    # Full Track Analysis
    curl -s -X POST "$API/analyze_full_track/$REQ_ID?lang=de" > /dev/null
    sleep 5
    
    # Fetch Results
    RESULT=$(curl -s "$API/analysis/$REQ_ID")
    LUFS=$(echo $RESULT | python3 -c "import sys, json; d=json.load(sys.stdin); print(d.get('summary', {}).get('measured_lufs', 'N/A'))")
    SCORE=$(echo $RESULT | python3 -c "import sys, json; d=json.load(sys.stdin); print(d.get('summary', {}).get('overall_score', 'N/A'))")
    
    echo "   -> ID: $REQ_ID | LUFS: $LUFS | Score: $SCORE"
done
