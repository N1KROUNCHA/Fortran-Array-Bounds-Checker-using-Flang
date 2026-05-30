const express = require('express');
const cors = require('cors');
const fs = require('fs');
const path = require('path');
const { exec } = require('child_process');

const app = express();
const PORT = process.env.PORT || 3000;

app.use(cors());
app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

// Create a temp directory for Fortran files within workspace
const TEMP_DIR = path.join(__dirname, 'temp');
if (!fs.existsSync(TEMP_DIR)) {
    fs.mkdirSync(TEMP_DIR, { recursive: true });
}

// Endpoint to run analysis using the compiled bounds-checker binary
app.post('/api/analyze', (req, res) => {
    const { code } = req.body;
    if (!code) {
        return res.status(400).json({ error: 'Code is required' });
    }

    const fileId = Date.now() + '_' + Math.random().toString(36).substr(2, 5);
    const tempFile = path.join(TEMP_DIR, `input_${fileId}.f90`);

    fs.writeFile(tempFile, code, (err) => {
        if (err) {
            console.error('Error writing temp file:', err);
            return res.status(500).json({ error: 'Failed to write input code to server' });
        }

        // Path to the bounds-checker binary
        const binaryPath = path.resolve(__dirname, '../bounds-checker');

        // Check if bounds-checker exists
        if (!fs.existsSync(binaryPath)) {
            // Try to build it if it doesn't exist
            console.log('Bounds-checker binary not found, attempting stand-alone compilation...');
            exec(`g++ -std=c++17 -O2 ../src/main_standalone.cpp -o ../bounds-checker`, { cwd: __dirname }, (buildErr) => {
                if (buildErr) {
                    cleanup(tempFile);
                    return res.status(500).json({
                        error: 'Compiler binary not found and standalone compile failed. Make sure you build it first using g++ or cmake.',
                        details: buildErr.message
                    });
                }
                runChecker(binaryPath, tempFile, res);
            });
        } else {
            runChecker(binaryPath, tempFile, res);
        }
    });
});

// Load original test cases so frontend can fetch them
app.get('/api/tests/:id', (req, res) => {
    const id = req.params.id; // e.g. "test01_constant_oob" or "test01"
    let filename = '';
    
    // Support short names or full names
    if (id.startsWith('test01') || id === '1') filename = 'test01_constant_oob.f90';
    else if (id.startsWith('test02') || id === '2') filename = 'test02_variable_index.f90';
    else if (id.startsWith('test03') || id === '3') filename = 'test03_correct_usage.f90';
    else if (id.startsWith('test04') || id === '4') filename = 'test04_mixed.f90';
    else if (id.startsWith('test05') || id === '5') filename = 'test05_const_expr.f90';

    if (!filename) {
        return res.status(400).json({ error: 'Invalid test case ID' });
    }

    const testPath = path.resolve(__dirname, '../testcases', filename);
    fs.readFile(testPath, 'utf8', (err, data) => {
        if (err) {
            console.error(`Error reading test file ${filename}:`, err);
            return res.status(404).json({ error: `Test file ${filename} not found` });
        }
        res.json({ filename, code: data });
    });
});

function runChecker(binary, file, res) {
    // Run the bounds-checker binary with the temporary file
    exec(`"${binary}" "${file}" --stats`, (error, stdout, stderr) => {
        // Cleanup file
        cleanup(file);

        // Note: C++ bounds-checker returns exit code 1 if there are constant OOB errors.
        // We do not treat this as a server failure. We return stdout/stderr normally.
        res.json({
            success: true,
            exitCode: error ? error.code : 0,
            stdout: stdout,
            stderr: stderr
        });
    });
}

function cleanup(filePath) {
    fs.unlink(filePath, (err) => {
        if (err) console.error('Error deleting temp file:', filePath, err);
    });
}

app.listen(PORT, () => {
    console.log(`Bounds Checker backend server running on http://localhost:${PORT}`);
});
