/**
 * ScareCrowWeb Configuration
 * 
 * Change API_URL to switch between local and cloud backends.
 * 
 * LOCAL DEVELOPMENT:
 *   window.SCARECROW_API_URL = 'http://localhost:8000';
 * 
 * CLOUD RUN DEPLOYMENT:
 *   window.SCARECROW_API_URL = 'https://your-service-name-xxxxxxxxxx-uc.a.run.app';
 */

// Default: Local development
window.SCARECROW_API_URL = 'http://localhost:8000';

// Uncomment below and add your Cloud Run URL when you deploy:
// window.SCARECROW_API_URL = 'https://scarecrow-api-xxxxxxxxxx-as.a.run.app';
