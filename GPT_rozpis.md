OSINT Workbench / Evidence Trace

Не «програма, яка сама знаходить все».

І не «Maltego з AliExpress» -_-.

Це має бути твій робочий простір для OSINT-розслідувань.

Умовно структура:

CASE
│
├── Entities
│   ├── Person
│   ├── Username
│   ├── Email
│   ├── Phone
│   ├── Domain
│   ├── Company
│   ├── Location
│   └── Image
│
├── Evidence
│   ├── URLs
│   ├── Screenshots
│   ├── Files
│   ├── Quotes
│   └── Metadata
│
├── Connections
│
├── Timeline
│
├── Notes
│
├── Playbooks
│
├── Tools / Modules
│
└── Report
Головна відмінність від SpiderFoot

SpiderFoot працює приблизно так:

domain.com
   ↓
50 OSINT modules
   ↓
300 results

І далі ти сидиш і розгрібаєш цей інформаційний пиздець.

Твоя програма працювала б інакше:

TARGET
   ↓
Investigation
   ↓
Facts + Evidence + Relations + Methodology
   ↓
Conclusion

Тобто вона не лише шукає, а допомагає думати.

Entities

Ти створив case:

Case: Example Investigation
Type: Person
Target: John Doe

Далі знаходиш:

Username:
john1337

Email:
john@example.com

Domain:
example.dev

GitHub:
github.com/john1337

І не пишеш це в .txt, Telegram Saved Messages, Obsidian, Excel та ще хуй знає куди.

Воно все лежить структуровано.

Evidence

Кожен факт має мати доказ.

Наприклад:

FACT

john1337 belongs to John Doe

До нього:

Source:
https://example.com/profile/john1337

Evidence:
Profile contains the same email

Captured:
2026-09-24 15:32

Confidence:
HIGH

Оце вже нормальний OSINT.

А не:

блять я точно пам'ятаю що десь бачив, що цей акаунт його

:)

Confidence

Дуже хочу, щоб ти це реалізував.

Наприклад:

UNVERIFIED
POSSIBLE
PROBABLE
CONFIRMED
FALSE

Тоді:

John
 │
 ├── john1337     CONFIRMED
 │
 ├── john1338     POSSIBLE
 │
 └── john1339     FALSE

Бо в реальному OSINT одна з найбільших проблем — припущення починають сприйматися як факти.

Relations / Graph

Тут беремо хорошу концепцію Maltego, але не намагаємось його переписати.

             ┌── GitHub account
             │
John ── Email ── Domain
 │
 ├── Username ── Forum
 │
 └── Company ── Website

Клікнув на node — справа:

ENTITY
john1337

Type:
Username

Confidence:
Confirmed

Sources:
3

Related:
John Doe
example.com
GitHub

Для твоєї програми граф — це спосіб навігації по case, а не головна програма.

А ось Playbooks — це, сука, найцікавіша частина

Це фактично твоя OSINT-пам'ять.

Наприклад ти створив playbook:

USERNAME INVESTIGATION

Всередині:

[ ] Search engines
[ ] GitHub
[ ] GitLab
[ ] Reddit
[ ] Forums
[ ] Wayback Machine
[ ] Search quoted username
[ ] Search username + email
[ ] Search avatar
[ ] Reverse image search avatar
[ ] Check metadata

І біля кожного пункту:

Technique:
Search exact username

Example:
"papapetruchio"

Queries:
"papapetruchio"
"papapetruchio" email
"papapetruchio" github

А потім одного дня ти знайшов новий ахуєнний метод.

Натискаєш:

Add Technique

і він залишається у твоєму playbook назавжди.

Через два роки ти вже матимеш не просто програму.

Ти матимеш свою OSINT knowledge base.

Modules

А сюди вже можна пхати автоматизацію.

Наприклад:

Username module
Domain module
Email module
Image module
Phone module
Metadata module
Face-search module

Наприклад натиснув на Domain:

example.com

і маєш кнопки:

RDAP
DNS
TLS Certificates
Wayback
Search Engine
Subdomains
WHOIS history

Частина відкриває зовнішній сервіс.

Частина робить запит сама.

І сюди прекрасно влазить твій PimEyes

Створив:

Entity:
IMAGE

натиснув:

Face Search

Програма відкриває/використовує PimEyes.

Результат:

Image match
↓
domain.com
↓
Add as evidence

І вже потім твоя система допомагає досліджувати сам домен.

Тобто твоя перша ідея не викидається.

Вона стає одним модулем другої.

Оце мені найбільше подобається.

Ще одна дуже важлива штука — Investigation Log

Автоматично вести:

15:32 Added john1337
15:33 Added source github.com/...
15:36 Marked relation John → john1337 as probable
15:44 Added email
15:51 Relation confirmed
16:03 Added screenshot

І бажано дозволити тобі самому писати:

How I found this:

Found username through old GitHub commit author.
Searched exact username.
Found same avatar on forum.
Reverse searched avatar.

Тоді через пів року ти відкриваєш case і повністю відновлюєш хід своїх думок.

Це дуже цінно.

А кінцева концепція для мене виглядає так
                 OSINT WORKBENCH
                       │
          ┌────────────┼─────────────┐
          │            │             │
       CASES        PLAYBOOKS      TOOLS
          │            │             │
     Investigation   Knowledge    Automation
          │
   ┌──────┼─────────┐
   │      │         │
Entities Evidence Relations
   │      │         │
   └──────┴────┬────┘
               │
            Timeline
               │
             Report

І найголовніше: не намагайся одразу зробити 150 OSINT-модулів. Це саме той шлях, де заїбешся через два тижні і проєкт здохне.

Перша реально корисна версія може мати лише:

Cases → Entities → Evidence → Relations → Playbooks → Investigation Log.

Навіть без автоматичного OSINT це вже буде софт, яким ти реально можеш користуватися.

А потім спокійно додаєш:

Domain tools → Username tools → Image → PimEyes → Metadata → Graph → Timeline → Export.

І через рік ця хуйня може вже бути дуже серйозним персональним OSINT environment, а не «лаба, яку я колись написав на другому курсі». (:
